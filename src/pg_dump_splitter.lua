local std, _ENV = _ENV

local lex = std.require 'lex'
local os_ext = std.require 'os_ext'
local sort_chunks = std.require 'sort_chunks'
local split_to_chunks = std.require 'split_to_chunks'

local export = {}

function export.make_default_options(options)
  return {
    lex_max_size = 16 * 1024 * 1024,
    io_size = 128 * 1024,
    lex_trans_more = false,
    lexemes_in_pt_ctx = false,
    save_unprocessed = false,
    no_schema_dirs = false,
    relaxed_order = false,
    sql_footer = '-- v' .. 'i:ts=2:sw=2:et',
    lex_consts = lex.consts,
    make_lex_ctx = lex.make_ctx,
    open = std.io.open,
    tmpfile = std.io.tmpfile,
    mkdir = os_ext.mkdir,
    remove = std.os.remove,
    rename = std.os.rename,
    ident_str_to_file_str = export.ident_str_to_file_str,
    make_add_to_chunk_options =
        sort_chunks.make_options_from_pg_dump_splitter,
    make_sort_rules_options =
        sort_chunks.make_options_from_pg_dump_splitter,
    make_pattern_rules_options =
        split_to_chunks.make_options_from_pg_dump_splitter,
    make_split_to_chunks_options =
        split_to_chunks.make_options_from_pg_dump_splitter,
    make_sort_chunk_options =
        sort_chunks.make_options_from_pg_dump_splitter,
    add_to_chunk = sort_chunks.add_to_chunk,
    make_sort_rules = sort_chunks.make_sort_rules,
    make_pattern_rules = split_to_chunks.make_pattern_rules,
    split_to_chunks = split_to_chunks.split_to_chunks,
    sort_chunk = sort_chunks.sort_chunk,
  }
end

export.chunks_ctx_proto = {}

function export.ident_str_to_file_str(ident)
  local file_str = ident:gsub('%c', '_'):gsub('%s', '_'):gsub('/', '_')
      :gsub('\\', '_'):gsub(':', '_'):gsub(';', '_'):gsub(',', '_'):sub(1, 160)

  if file_str:sub(1, 1) == '.' then
    file_str = '_' .. file_str
  end

  return file_str
end

function export.chunks_ctx_proto:add(obj_type, obj_values, dump_data)
  local skip
  local rule = self.sort_rules[obj_type]

  std.assert(obj_type, 'no obj_type')
  std.assert(rule, 'no sort rule for obj_type: ' .. obj_type)

  local directories, filename, order = rule:handler(obj_type, obj_values)

  if self.hooks_ctx.add_to_chunk_handler then
    directories, filename, order, dump_data =
        self.hooks_ctx:add_to_chunk_handler(obj_type, obj_values, directories,
            filename, order, dump_data)
  end

  if not directories or not filename or not order or not dump_data then
    return
  end

  local raw_path, ready_path = self.options.add_to_chunk(
      self.output_dir, directories, filename, order, dump_data,
      self.options:make_add_to_chunk_options())

  if self.hooks_ctx.added_to_chunk_handler then
    self.hooks_ctx:added_to_chunk_handler(obj_type, obj_values,
        self.output_dir, directories, filename, order, dump_data,
        raw_path, ready_path)
  end

  local buf = ('ss'):pack(raw_path, ready_path)
  self.paths_fd:write(('j'):pack(#buf), buf)
end

function export.pg_dump_splitter(dump_path, output_dir, hooks_path, options)
  local hooks_ctx = {}

  if hooks_path then
    local hooks_lib_func = std.assert(std.loadfile(hooks_path))
    local hooks_lib = hooks_lib_func(hooks_path)

    hooks_lib.register_hooks(hooks_ctx)
  end

  if hooks_ctx.options_handler then
    hooks_ctx:options_handler(options)
  end

  local lex_ctx
  local dump_fd
  local paths_fd

  local ok, err = std.xpcall(function()
    if hooks_ctx.begin_program_handler then
      hooks_ctx:begin_program_handler(dump_path, output_dir)
    end

    local tmp_output_dir = output_dir .. '.part'

    if hooks_ctx.tmp_output_dir_handler then
      tmp_output_dir = hooks_ctx:tmp_output_dir_handler(tmp_output_dir)
    end

    std.assert(tmp_output_dir, 'no tmp_output_dir')

    lex_ctx = options.make_lex_ctx(options.lex_max_size)
    dump_fd = std.assert(options.open(dump_path, 'rb'))
    paths_fd = std.assert(options.tmpfile())
    std.assert(options.mkdir(tmp_output_dir))

    if hooks_ctx.made_output_dir_handler then
      hooks_ctx:made_output_dir_handler(tmp_output_dir)
    end

    local sort_rules = options.make_sort_rules(
        options:make_sort_rules_options())

    local pattern_rules = options.make_pattern_rules(
        options:make_pattern_rules_options())

    local chunks_ctx = std.setmetatable(
      {
        sort_rules = sort_rules,
        output_dir = tmp_output_dir,
        hooks_ctx = hooks_ctx,
        options = options,
        paths_fd = paths_fd,
      },
      {__index = export.chunks_ctx_proto}
    )

    if hooks_ctx.begin_split_to_chunks_handler then
      hooks_ctx:begin_split_to_chunks_handler(lex_ctx, dump_fd,
          pattern_rules, chunks_ctx)
    end

    options.split_to_chunks(lex_ctx, dump_fd,
        pattern_rules, chunks_ctx, hooks_ctx,
        options:make_split_to_chunks_options())

    if hooks_ctx.end_split_to_chunks_handler then
      hooks_ctx:end_split_to_chunks_handler()
    end

    paths_fd:seek('set', 0)

    if hooks_ctx.begin_sort_chunks_handler then
      hooks_ctx:begin_sort_chunks_handler(tmp_output_dir)
    end

    while true do
      local buf = paths_fd:read(('j'):packsize())

      if not buf then break end

      buf = paths_fd:read((('j'):unpack(buf)))
      local raw_path, ready_path = ('ss'):unpack(buf)

      local ready_fd = options.open(ready_path, 'rb')

      if ready_fd then
        ready_fd:close()
        goto sort_continue
      end

      options.sort_chunk(raw_path, ready_path,
          options:make_sort_chunk_options())

      if hooks_ctx.sorted_chunk_handler then
        hooks_ctx:sorted_chunk_handler(raw_path, ready_path)
      end

      ::sort_continue::
    end

    if hooks_ctx.end_sort_chunks_handler then
      hooks_ctx:end_sort_chunks_handler()
    end

    std.assert(options.rename(tmp_output_dir, output_dir))

    if hooks_ctx.renamed_output_dir_handler then
      hooks_ctx:renamed_output_dir_handler(tmp_output_dir, output_dir)
    end
    if hooks_ctx.end_program_handler then
      hooks_ctx:end_program_handler()
    end
  end, std.debug.traceback)

  if paths_fd then paths_fd:close() end
  if dump_fd then dump_fd:close() end
  if lex_ctx then lex_ctx:free() end

  std.assert(ok, err)
end

return export

-- vi:ts=2:sw=2:et
