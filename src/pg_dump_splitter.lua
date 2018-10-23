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
    no_schema_dirs = false,
    relaxed_order = false,
    lex_consts = lex.consts,
    make_lex_ctx = lex.make_ctx,
    open = std.io.open,
    mkdir = os_ext.mkdir,
    rename = std.os.rename,
    make_add_to_chunk_options =
        sort_chunks.make_options_from_pg_dump_splitter,
    make_sort_rules_options =
        sort_chunks.make_options_from_pg_dump_splitter,
    make_sort_chunk_options =
        sort_chunks.make_options_from_pg_dump_splitter,
    make_pattern_rules_options =
        split_to_chunks.make_options_from_pg_dump_splitter,
    make_split_to_chunks_options =
        split_to_chunks.make_options_from_pg_dump_splitter,
    add_to_chunk = sort_chunks.add_to_chunk,
    make_sort_rules = sort_chunks.make_sort_rules,
    make_pattern_rules = split_to_chunks.make_pattern_rules,
    split_to_chunks = split_to_chunks.split_to_chunks,
    sort_chunk = sort_chunks.sort_chunk,
  }
end

export.chunks_ctx_proto = {}

function export.chunks_ctx_proto:add(obj_type, obj_values, dump_data)
  local skip
  local rule = self.sort_rules[obj_type]

  std.assert(obj_type, 'no obj_type')
  std.assert(rule, 'no sort rule for obj_type: ' .. obj_type)

  local directories, filename, order = rule:handler(obj_type, obj_values)

  if self.hooks_ctx.add_to_chunk_handler then
    directories, filename, order = self.hooks_ctx:add_to_chunk_handler(
        obj_type, obj_values, directories, filename, order, dump_data)
  end

  if not directories or not filename or not order then
    return
  end

  local raw_path, ready_path = self.options.add_to_chunk(
      directories, filename, order, dump_data,
      self.options:make_add_to_chunk_options())

  if self.hooks_ctx.added_to_chunk_handler then
    self.hooks_ctx:added_to_chunk_handler(obj_type, obj_values, directories,
        filename, order, dump_data, raw_path, ready_path)
  end

  std.table.insert(self.paths, {raw_path, ready_path})
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

  local ok, err = std.xpcall(function()
    if hooks_ctx.begin_program_handler then
      hooks_ctx:begin_program_handler(dump_path, output_dir)
    end

    local tmp_output_dir = output_dir .. '.part'

    if hooks_ctx.tmp_output_dir_handler then
      tmp_output_dir = hooks_ctx:tmp_output_dir_handler(tmp_output_dir)

      std.assert(tmp_output_dir, 'no tmp_output_dir')
    end

    lex_ctx = options.make_lex_ctx(options.lex_max_size)

    local sort_rules = options.make_sort_rules(
        options:make_sort_rules_options())

    local pattern_rules = options.make_pattern_rules(
        options:make_pattern_rules_options())

    local paths = {}

    local chunks_ctx = std.setmetatable(
      {
        sort_rules = sort_rules,
        output_dir = tmp_output_dir,
        hooks_ctx = hooks_ctx,
        options = options,
        paths = paths,
      },
      {__index = export.chunks_ctx_proto}
    )

    dump_fd = std.assert(options.open(dump_path))
    std.assert(options.mkdir(tmp_output_dir))

    if hooks_ctx.made_output_dir_handler then
      hooks_ctx:made_output_dir_handler(tmp_output_dir)
    end
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
    if hooks_ctx.begin_sort_chunks_handler then
      hooks_ctx:begin_sort_chunks_handler(tmp_output_dir)
    end

    for i, v in std.ipairs(paths) do
      local raw_path, ready_path = std.table.unpack(v)

      options.sort_chunk(raw_path, ready_path,
          options:make_sort_chunk_options())

      if hooks_ctx.sorted_chunk_handler then
        hooks_ctx:sorted_chunk_handler(raw_path, ready_path)
      end
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

  if dump_fd then dump_fd:close() end
  if lex_ctx then lex_ctx:free() end

  std.assert(ok, err)
end

return export

-- vi:ts=2:sw=2:et
