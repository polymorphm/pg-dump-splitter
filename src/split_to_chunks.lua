local std, _ENV = _ENV

local export = {}

function export.make_options_from_pg_dump_splitter(options)
  return {
    read_size = options.read_size,
    lex_consts = options.lex_consts,
    lex_trans_more = options.lex_trans_more,
    make_lex_ctx = options.make_lex_ctx,
    open = options.open,
    mkdir = options.mkdir,
  }
end

function export.lex_ctx_iter_item(iter_ctx)
  while true do
    if #iter_ctx.items > 0 then
      return std.table.unpack(std.table.remove(iter_ctx.items))
    end

    if iter_ctx.final then return end

    local buf = iter_ctx.dump_fd:read(iter_ctx.options.read_size)
    local items = {}
    local function yield(...)
      std.table.insert(items, std.table.pack(...))
    end

    iter_ctx.lex_ctx:feed(buf, yield, iter_ctx.options.lex_trans_more)

    while true do
      local item = std.table.remove(items)
      if not item then break end
      std.table.insert(iter_ctx.items, item)
    end

    if not buf then
      iter_ctx.final = true
    end
  end
end

function export.lex_ctx_iter(lex_ctx, dump_fd, options)
  local iter_ctx = {
    lex_ctx = lex_ctx,
    dump_fd = dump_fd,
    options = options,
    final = false,
    items = {}
  }

  return export.lex_ctx_iter_item, iter_ctx
end

function export.make_pattern_rules(options)
  return {
    {
      'create_function',
      {'kw', 'create'},
      {
        'fork',
        {
          {'kw', 'or'},
          {'kw', 'replace'},
        },
        {},
      },
      {'kw', 'function'},
      {
        'fork',
        {
          {'ident', 'create_function_obj_schema'},
          {'ss', '.'},
          {'ident', 'create_function_obj_name'},
        },
        {
          {'ident', 'create_function_obj_name_wo_schema'},
        },
      },
      {'ss', '('},
      {'any'},
      {'ss', ')'},
      {'any'},
      {'end'},
    },

    {
      'function_owner',
      {'kw', 'alter'},
      {'kw', 'function'},
      {
        'fork',
        {
          {'ident', 'function_owner_obj_schema'},
          {'ss', '.'},
          {'ident', 'function_owner_obj_name'},
        },
        {
          {'ident', 'function_owner_obj_name_wo_schema'},
        },
      },
      {'ss', '('},
      {'any'},
      {'ss', ')'},
      {'any'},
      {'end'},
    },

    -- TODO ... ... ...
  }
end

function export.process_pt_ctx(pt_ctx, lex_type, lex_subtype, location,
    value, translated_value, level, options)
  local next_pts = {}
  local queue_pts
  local next_queue_pts = pt_ctx.pts

  repeat
    queue_pts = next_queue_pts
    next_queue_pts = {}

    for pt_i, pt in std.ipairs(queue_pts) do
      local cand_obj_type = pt[1]
      local rule = pt[2]

      if not rule then
        goto continue
      end

      local pt_type = rule[1]
      local pt_arg = rule[2]

      if pt_type == 'kw' then
        if level ~= 0 or
            lex_type ~= options.lex_consts.type_ident or
            translated_value ~= pt_arg then
          goto continue
        end

        std.table.insert(next_pts, {
          cand_obj_type,
          std.select(3, std.table.unpack(pt)),
        })
      elseif pt_type == 'ss' then
        if level ~= 0 or
            lex_subtype ~= options.lex_consts.subtype_special_symbols or
            value ~= pt_arg then
          goto continue
        end

        std.table.insert(next_pts, {
          cand_obj_type,
          std.select(3, std.table.unpack(pt)),
        })
      elseif pt_type == 'ident' then
        if level ~= 0 or
            lex_type ~= options.lex_consts.type_ident then
          goto continue
        end

        pt_ctx[pt_arg] = translated_value

        std.table.insert(next_pts, {
          cand_obj_type,
          std.select(3, std.table.unpack(pt)),
        })
      elseif pt_type == 'end' then
        if level ~= 0 or
            lex_subtype ~= options.lex_consts.subtype_special_symbols or
            value ~= ';' then
          goto continue
        end

        if pt_ctx.cand_obj_type and
            pt_ctx.cand_obj_type ~= cand_obj_type then
          std.error('pos(' .. pt_ctx.location.lpos ..
              ') line(' .. pt_ctx.location.lline ..
              ') col(' .. pt_ctx.location.lcol ..
              '): ambiguous obj_type: ' .. pt_ctx.cand_obj_type ..
              ' or ' .. cand_obj_type)
        else
          pt_ctx.cand_obj_type = cand_obj_type
        end
      elseif pt_type == 'any' then
        std.table.insert(next_pts, pt)

        std.table.insert(next_queue_pts, {
          cand_obj_type,
          std.select(3, std.table.unpack(pt)),
        })
      elseif pt_type == 'fork' then
        for fork_rules_i = 2, #rule do
          local fork_rules = rule[fork_rules_i]

          if #fork_rules == 0 then
            std.table.insert(next_queue_pts, {
              cand_obj_type,
              std.select(3, std.table.unpack(pt)),
            })
          else
            local next_pt = {
              cand_obj_type,
              std.table.unpack(fork_rules),
            }
            for i = 3, #pt do
              std.table.insert(next_pt, pt[i])
            end

            std.table.insert(next_queue_pts, next_pt)
          end
        end
      else
        if pt_type then
          std.error('invalid pt_type: ' .. pt_type)
        else
          std.error('invalid pt_type (nil)')
        end
      end

      ::continue::
    end
  until #next_queue_pts == 0

  pt_ctx.pts = next_pts
end

function export.extract_dump_data(dump_fd, begin_pos, end_pos)
  local saved_pos = dump_fd:seek()
  dump_fd:seek('set', begin_pos - 1) -- begin_pos is 1 based
  local data = dump_fd:read(end_pos - begin_pos)
  dump_fd:seek('set', saved_pos)

  return data
end

function export.split_to_chunks(lex_ctx, dump_fd, dump_path, output_dir,
    hooks_ctx, options)
  local pattern_rules = export.make_pattern_rules(options)
  local level = 0
  local pt_ctx

  for lex_type, lex_subtype, location, value, translated_value
      in export.lex_ctx_iter(lex_ctx, dump_fd, options) do
    if lex_subtype == options.lex_consts.subtype_special_symbols and
          value == ')' then
      level = level - 1

      if level < 0 then
        std.error('pos(' .. location.lpos .. ') line(' .. location.lline ..
            ') col(' .. location.lcol .. '): level < 0')
      end
    end

    if hooks_ctx.lexeme_handler then
      lex_type, lex_subtype, location, value, translated_value =
          hooks_ctx:lexeme_handler(
              lex_type, lex_subtype, location, value, translated_value,
              level)

      std.assert(lex_type, 'no lex_type')
    end

    if lex_type ~= options.lex_consts.type_comment then
      local end_pos = location.lpos + #value

      if not pt_ctx then
        pt_ctx = {
          pts = pattern_rules,
          location = location,
        }
      end

      export.process_pt_ctx(pt_ctx, lex_type, lex_subtype, location,
          value, translated_value, level, options)

      if lex_subtype == options.lex_consts.subtype_special_symbols and
          level == 0 and value == ';' then
        local dump_data = export.extract_dump_data(dump_fd,
            pt_ctx.location.lpos, end_pos)
        local skip = false

        if pt_ctx.cand_obj_type then

          if hooks_ctx.processed_pt_handler then
            skip = hooks_ctx:processed_pt_handler(pt_ctx, skip, dump_data)
          end

          if not skip then
            -- TODO ... ... ...

          end
        else
          if hooks_ctx.unprocessed_pt_handler then
            skip = hooks_ctx:unprocessed_pt_handler(pt_ctx, skip, dump_data,
                pt_ctx.error_dump_data)
          end

          if not skip then
              std.error('pos(' .. pt_ctx.location.lpos .. ') line(' ..
                  pt_ctx.location.lline ..
                  ') col(' .. pt_ctx.location.lcol ..
                  '): unprocessed pattern:\n' .. dump_data ..
                  '\n----------\n' .. pt_ctx.error_dump_data)
          end
        end

        pt_ctx = nil
      elseif #pt_ctx.pts == 0 and not pt_ctx.error_dump_data then
        pt_ctx.error_dump_data = export.extract_dump_data(dump_fd,
            pt_ctx.location.lpos, end_pos)
      end
    end

    if lex_subtype == options.lex_consts.subtype_special_symbols and
        value == '(' then
      level = level + 1
    end
  end

  if pt_ctx then
    local dump_data = export.extract_dump_data(dump_fd,
        pt_ctx.location.lpos, dump_fd:seek() + 1)
    local skip = false

    if hooks_ctx.unprocessed_eof_pt_handler then
      skip = hooks_ctx:unprocessed_eof_pt_handler(pt_ctx, skip, dump_data)
    end

    if not skip then
        std.error('pos(' .. pt_ctx.location.lpos .. ') line(' ..
            pt_ctx.location.lline .. ') col(' .. pt_ctx.location.lcol ..
            '): unprocessed pattern at EOF: ' .. dump_data)
    end
  end
end

return export

-- vi:ts=2:sw=2:et
