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

function export.split_to_chunks(lex_ctx, dump_fd, dump_path, output_dir,
    hooks_ctx, options)
  for lex_type, lex_subtype, location, value, translated_value
      in export.lex_ctx_iter(lex_ctx, dump_fd, options) do
    std.print('+++', lex_type, lex_subtype--[[, location, value--]], translated_value, '---') -- DEBUG ONLY
  end
end

return export

-- vi:ts=2:sw=2:et
