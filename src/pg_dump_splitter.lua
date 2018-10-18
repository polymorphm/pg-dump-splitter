-- vim: set et ts=2 sw=2:

local std, _ENV = _ENV

local lex = std.require 'lex'
local os_ext = std.require 'os_ext'

--local function split_to_chunks(
--    lex_ctx, dump_fd, dump_path, output_dir, options, hooks_ctx, options)
--    TODO    detach to separete library file
--end

local function init_default_options(options)
  options.lex_max_size = 16 * 1024 * 1024
  options.lex_consts = lex.consts
  options.make_lex_ctx = lex.make_ctx
  options.open = std.io.open
  options.mkdir = os_ext.mkdir
  options.rename = std.os.rename
  --options.split_to_chunks = XXXXXXX.split_to_chunks
  --options.sort_chunks = XXXXXXX.sort_chunks
end

local function make_default_options()
  local options = {}
  init_default_options(options)
  return options
end

local function pg_dump_splitter(dump_path, output_dir, hooks_path, options)
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
    dump_fd = std.assert(options.open(dump_path))
    std.assert(options.mkdir(tmp_output_dir))

    if hooks_ctx.made_output_dir_handler then
      hooks_ctx:made_output_dir_handler(tmp_output_dir)
    end
    if hooks_ctx.begin_split_to_chunks_handler then
      hooks_ctx:begin_split_to_chunks_handler(
          lex_ctx, dump_fd, dump_path, tmp_output_dir)
    end

    --options.split_to_chunks(
    --    lex_ctx, dump_fd, dump_path, tmp_output_dir, hooks_ctx, options)

    if hooks_ctx.end_split_to_chunks_handler then
      hooks_ctx:end_split_to_chunks_handler()
    end
    if hooks_ctx.begin_sort_chunks_handler then
      hooks_ctx:begin_sort_chunks_handler(tmp_output_dir)
    end

    --options.sort_chunks(tmp_output_dir, hooks_ctx, options)

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

return {
  init_default_options = init_default_options,
  make_default_options = make_default_options,
  pg_dump_splitter = pg_dump_splitter,
}
