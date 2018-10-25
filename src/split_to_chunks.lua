local std, _ENV = _ENV

local export = {}

function export.make_options_from_pg_dump_splitter(options)
  return {
    io_size = options.io_size,
    lex_consts = options.lex_consts,
    lex_trans_more = options.lex_trans_more,
    make_pattern_rules = options.make_pattern_rules,
    lexemes_in_pt_ctx = options.lexemes_in_pt_ctx,
    save_unprocessed = options.save_unprocessed,
  }
end

function export.lex_ctx_iter_item(iter_ctx)
  while true do
    if #iter_ctx.items > 0 then
      return std.table.unpack(std.table.remove(iter_ctx.items))
    end

    if iter_ctx.final then return end

    local buf = iter_ctx.dump_fd:read(iter_ctx.options.io_size)
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
  local kw = export.kw_rule_handler
  local ss = export.ss_rule_handler
  local ident = export.make_ident_rule_handler()
  local str = export.str_rule_handler
  local en = export.en_rule_handler
  local any =  export.any_rule_handler
  local fork = export.fork_rule_handler
  local rep = export.rep_rule_handler

  return {
    {
      'set',
      {kw, 'set'},
      {any},
      {en},
    },

    {
      'select',
      {kw, 'select'},
      {any},
      {en},
    },

    {
      'create_schema',
      {kw, 'create'},
      {kw, 'schema'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_schema',
      {kw, 'alter'},
      {kw, 'schema'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'create_extension',
      {kw, 'create'},
      {kw, 'extension'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_extension',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'extension'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_type',
      {kw, 'create'},
      {kw, 'type'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'as'},
      {any},
      {en},
    },

    {
      'alter_type',
      {kw, 'alter'},
      {kw, 'type'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_type',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'type'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_function',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'or'},
          {kw, 'replace'},
        },
        {},
      },
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'alter_function',
      {kw, 'alter'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'create_table',
      {kw, 'create'},
      {
        rep,
        {
          fork,
          {
            {kw, 'global'}
          },
          {
            {kw, 'local'}
          },
          {
            {kw, 'temporary'}
          },
          {
            {kw, 'temp'}
          },
          {
            {kw, 'unlogged'}
          },
        },
      },
      {kw, 'table'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {
        fork,
        {
          {kw, 'partition'},
          {kw, 'of'},
          {
            fork,
            {
              {ident, 'parent_schema'},
              {ss, '.'},
            },
            {},
          },
          {ident, 'parent_name'},
        },
        {
          {kw, 'of'},
          {
            fork,
            {
              {ident, 'type_schema'},
              {ss, '.'},
            },
            {},
          },
          {ident, 'type_name'},
        },
        {},
      },
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'alter_table',
      {kw, 'alter'},
      {kw, 'table'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'exists'},
        },
        {},
      },
      {
        fork,
        {
          {kw, 'only'},
        },
        {},
      },
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'create_sequence',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'temporary'}
        },
        {
          {kw, 'temp'}
        },
        {},
      },
      {kw, 'sequence'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_sequence',
      {kw, 'alter'},
      {kw, 'sequence'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'exists'},
        },
        {},
      },
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'create_index',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'unique'},
        },
        {},
      },
      {kw, 'index'},
      {
        fork,
        {
          {kw, 'concurrently'},
        },
        {},
      },
      {
        fork,
        {
          {
            fork,
            {
              {kw, 'if'},
              {kw, 'not'},
              {kw, 'exists'},
            },
            {},
          },
          {ident, 'obj_name'},
        },
        {},
      },
      {kw, 'on'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {any},
      {en},
    },

    {
      'grant_schema',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {kw, 'schema'},
      {ident, 'obj_name'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'revoke_function',
      {kw, 'revoke'},
      {any},
      {kw, 'on'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {kw, 'from'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'grant_function',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },
  }
end

export.rule_ctx_proto = {}

function export.rule_ctx_proto:put_value(key, value)
  if not self.value_version then self.value_version = {} end

  self.value_version[key] = value
end

function export.rule_ctx_proto:dup_value_version(to_pt)
  if not self.value_version then return end

  local next_val_ver = {}
  self.pt_ctx.value_versions[to_pt] = next_val_ver

  for k, v in std.pairs(self.value_version) do
    next_val_ver[k] = v
  end
end

function export.rule_ctx_proto:push_pt(next_pt)
  std.table.insert(self.next_pts, next_pt)
  self:dup_value_version(next_pt)
end

function export.rule_ctx_proto:push_pt_soon(next_pt)
  std.table.insert(self.next_pts_soon, next_pt)
  self:dup_value_version(next_pt)
end

function export.rule_ctx_proto:make_shifted_pt()
  return std.table.move(self.pt, 3, #self.pt, 2, {self.obj_type})
end

function export.rule_ctx_proto:push_shifted_pt()
  local next_pt = self:make_shifted_pt()
  self:push_pt(next_pt)
end

function export.kw_rule_handler(rule_ctx, lexeme, options)
  if lexeme.level == 1 and
      lexeme.lex_subtype == options.lex_consts.subtype_simple_ident and
      lexeme.translated_value == rule_ctx.rule[2] then
    rule_ctx:push_shifted_pt()
  end
end

function export.ss_rule_handler(rule_ctx, lexeme, options)
  if lexeme.level == 1 and
      lexeme.lex_subtype == options.lex_consts.subtype_special_symbols and
      lexeme.value == rule_ctx.rule[2] then
    rule_ctx:push_shifted_pt()
  end
end

function export.make_ident_rule_handler()
  -- reserved key words got from
  -- https://www.postgresql.org/docs/11/static/sql-keywords-appendix.html
  local reserved_kw = {'all', 'analyse', 'analyze', 'and', 'any', 'array', 'as',
      'asc', 'asymmetric', 'both', 'case', 'cast', 'check', 'collate',
      'column', 'constraint', 'create', 'current_catalog', 'current_date',
      'current_role', 'current_time', 'current_timestamp', 'current_user',
      'default', 'deferrable', 'desc', 'distinct', 'do', 'else', 'end',
      'except', 'false', 'fetch', 'for', 'foreign', 'from', 'grant', 'group',
      'having', 'in', 'initially', 'intersect', 'into', 'lateral', 'leading',
      'limit', 'localtime', 'localtimestamp', 'not', 'null', 'offset', 'on',
      'only', 'or', 'order', 'placing', 'primary', 'references', 'returning',
      'select', 'session_user', 'some', 'symmetric', 'table', 'then', 'to',
      'trailing', 'true', 'union', 'unique', 'user', 'using', 'variadic',
      'when', 'where', 'window', 'with'}

  local reserved_kw_set = {}
  for i, v in std.ipairs(reserved_kw) do reserved_kw_set[v] = true end

  return function(rule_ctx, lexeme, options)
    if lexeme.level == 1 and
        lexeme.lex_type == options.lex_consts.type_ident and
        not (lexeme.lex_subtype == options.lex_consts.subtype_simple_ident and
            reserved_kw_set[lexeme.translated_value]) then
      rule_ctx:put_value(rule_ctx.rule[2], lexeme.translated_value)
      rule_ctx:push_shifted_pt()
    end
  end
end

function export.str_rule_handler(rule_ctx, lexeme, options)
  if lexeme.level == 1 and
      lexeme.lex_type == options.lex_consts.type_string then
    rule_ctx:put_value(rule_ctx.rule[2], lexeme.translated_value)
    rule_ctx:push_shifted_pt()
  end
end

function export.en_rule_handler(rule_ctx, lexeme, options)
  if lexeme.level == 1 and
      lexeme.lex_subtype == options.lex_consts.subtype_special_symbols and
      lexeme.value == ';' then
    if rule_ctx.pt_ctx.obj_type and
        rule_ctx.pt_ctx.obj_type ~= rule_ctx.obj_type then
      std.error('pos(' .. rule_ctx.pt_ctx.location.lpos ..
          ') line(' .. rule_ctx.pt_ctx.location.lline ..
          ') col(' .. rule_ctx.pt_ctx.location.lcol ..
          '): ambiguous obj_type: ' .. rule_ctx.pt_ctx.obj_type ..
          ' or ' .. rule_ctx.obj_type)
    elseif not rule_ctx.pt_ctx.obj_type then
      rule_ctx.pt_ctx.obj_type = rule_ctx.obj_type
      rule_ctx.pt_ctx.obj_values = rule_ctx.value_version or {}
    end
  end
end

function export.any_rule_handler(rule_ctx, lexeme, options)
  local next_pt = rule_ctx:make_shifted_pt()

  rule_ctx:push_pt_soon(next_pt)
  rule_ctx:push_pt(rule_ctx.pt)
end

function export.fork_rule_handler(rule_ctx, lexeme, options)
  for i = 2, #rule_ctx.rule do
    local fork_rules = rule_ctx.rule[i]
    local next_pt = std.table.move(fork_rules, 1, #fork_rules,
        2, {rule_ctx.obj_type})
    std.table.move(rule_ctx.pt, 3, #rule_ctx.pt, #next_pt + 1, next_pt)

    rule_ctx:push_pt_soon(next_pt)
  end
end

function export.rep_rule_handler(rule_ctx, lexeme, options)
  -- rule: repeat a sequence (in arguments) zero, one or several times

  local next_pt_with_seq = std.table.move(rule_ctx.rule, 2, #rule_ctx.rule,
      2, {rule_ctx.obj_type})
  std.table.insert(next_pt_with_seq, rule_ctx.rule)
  std.table.move(rule_ctx.pt, 3, #rule_ctx.pt,
      #next_pt_with_seq + 1, next_pt_with_seq)

  local next_pt_wo_seq = rule_ctx:make_shifted_pt()

  rule_ctx:push_pt_soon(next_pt_with_seq)
  rule_ctx:push_pt_soon(next_pt_wo_seq)
end

function export.process_pt_ctx(pt_ctx, lex_type, lex_subtype, location,
    value, translated_value, level, options)
  local next_pts = {}
  local next_pts_soon = pt_ctx.pts

  local lexeme = {
    lex_type = lex_type,
    lex_subtype = lex_subtype,
    location = location,
    value = value,
    translated_value = translated_value,
    level = level,
  }

  if options.lexemes_in_pt_ctx then
    if not pt_ctx.lexemes then pt_ctx.lexemes = {} end
    std.table.insert(pt_ctx.lexemes, lexeme)
  end

  repeat
    local pts = next_pts_soon
    next_pts_soon = {}

    for pt_i, pt in std.ipairs(pts) do
      local obj_type = pt[1]
      local rule = pt[2]

      if not rule then
        goto continue
      end

      local rule_handler = rule[1]
      local rule_ctx = std.setmetatable(
        {
          pt_ctx = pt_ctx,
          next_pts = next_pts,
          next_pts_soon = next_pts_soon,
          pt = pt,
          obj_type = obj_type,
          rule = rule,
          value_version = pt_ctx.value_versions[pt],
        },
        {__index = export.rule_ctx_proto}
      )

      rule_handler(rule_ctx, lexeme, options)

      ::continue::
    end
  until #next_pts_soon == 0

  pt_ctx.pts = next_pts
end

function export.extract_dump_data(dump_fd, begin_pos, end_pos)
  local saved_pos = dump_fd:seek()
  dump_fd:seek('set', begin_pos - 1) -- begin_pos is 1 based
  local data = dump_fd:read(end_pos - begin_pos)
  dump_fd:seek('set', saved_pos)

  return data
end

function export.split_to_chunks(lex_ctx, dump_fd, pattern_rules,
    chunks_ctx, hooks_ctx, options)
  local level = 1
  local pt_ctx

  for lex_type, lex_subtype, location, value, translated_value
      in export.lex_ctx_iter(lex_ctx, dump_fd, options) do
    if lex_subtype == options.lex_consts.subtype_special_symbols and
          value == ')' then
      level = level - 1

      if level < 1 then
        std.error('pos(' .. location.lpos .. ') line(' .. location.lline ..
            ') col(' .. location.lcol .. '): level < 1')
      end
    end

    if hooks_ctx.lexeme_handler then
      lex_type, lex_subtype, location, value, translated_value, level =
          hooks_ctx:lexeme_handler(
              lex_type, lex_subtype, location, value, translated_value, level)

    end

    std.assert(lex_type, 'no lex_type')
    std.assert(lex_subtype, 'no lex_subtype')
    std.assert(location, 'no location')
    std.assert(value, 'no value')
    std.assert(level, 'no level')

    if lex_type ~= options.lex_consts.type_comment then
      local end_pos = location.lpos + #value

      if not pt_ctx then
        pt_ctx = {
          pts = pattern_rules,
          location = location,
          value_versions = std.setmetatable({}, {__mode = 'k'}),
        }
      end

      export.process_pt_ctx(pt_ctx, lex_type, lex_subtype, location,
          value, translated_value, level, options)

      if lex_subtype == options.lex_consts.subtype_special_symbols and
          level == 1 and value == ';' then
        local dump_data = export.extract_dump_data(dump_fd,
            pt_ctx.location.lpos, end_pos)
        local skip

        if pt_ctx.obj_type then

          if hooks_ctx.processed_pt_handler then
            skip = hooks_ctx:processed_pt_handler(pt_ctx, dump_data)
          else
            skip = false
          end

          if not skip then
            chunks_ctx:add(pt_ctx.obj_type, pt_ctx.obj_values, dump_data)
          end
        else
          if hooks_ctx.unprocessed_pt_handler then
            skip = hooks_ctx:unprocessed_pt_handler(pt_ctx, dump_data,
                pt_ctx.error_dump_data)
          else
            skip = options.save_unprocessed
          end

          if not skip then
            local error_dump_data = pt_ctx.error_dump_data or '(no error_dump_data)'

            std.error('pos(' .. pt_ctx.location.lpos .. ') line(' ..
                pt_ctx.location.lline ..
                ') col(' .. pt_ctx.location.lcol ..
                '): unprocessed pattern:\n' .. dump_data ..
                '\n' .. ('-'):rep(60) .. '\n' ..
                error_dump_data)
          end

          chunks_ctx:add(pt_ctx.obj_type or 'unprocessed',
              pt_ctx.obj_values or {}, dump_data)
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
    local skip

    if hooks_ctx.unprocessed_eof_pt_handler then
      skip = hooks_ctx:unprocessed_eof_pt_handler(pt_ctx, dump_data)
    else
      skip = false
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
