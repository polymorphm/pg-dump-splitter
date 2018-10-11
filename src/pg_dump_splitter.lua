-- vim: set et ts=4 sw=4:

local e, _ENV = _ENV

--local lex = e.require 'lex'
--local os_ext = e.require 'os_ext'

--local function split_to_chunks(
--        lex_ctx, dump_fd, dump_path, output_dir, options, hooks_ctx, options)
--   TODO     detach to separete library file
--end

local function make_default_options()
    return {
        --split_to_chunks = split_to_chunks,
    }
end

local function pg_dump_splitter(dump_path, output_dir, hooks_path, options)
    if not options then options = make_default_options() end

    local hooks_ctx = {}

    if hooks_path then
        local hooks_lib_func = e.assert(e.loadfile(hooks_path))
        local hooks_lib = hooks_lib_func(hooks_path)

        hooks_lib.register_hooks(hooks_ctx)
    end

    if hooks_ctx.options_handler then
        hooks_ctx:options_handler(options)
    end

    local lex_ctx
    local dump_fd

    local res, err = e.xpcall(function()
        if hooks_ctx.begin_program_handler then
            hooks_ctx:begin_program_handler(dump_path, output_dir)
        end

        local tmp_output_dir = 'asdasdasd' -- TODO !!!!!!!!

        if hooks_ctx.tmp_output_dir_handler then
            tmp_output_dir = hooks_ctx:tmp_output_dir_handler(tmp_output_dir)

            e.assert(tmp_output_dir, 'no tmp_output_dir')
        end

        --lex_ctx = 
        dump_fd = e.assert(e.io.open(dump_path))
        --os_ext.mkdir(tmp_output_dir)

        if hooks_ctx.made_output_dir_handler then
            hooks_ctx:made_output_dir_handler(tmp_output_dir)
        end
        if hooks_ctx.begin_split_to_chunks_handler then
            hooks_ctx:begin_split_to_chunks_handler(
                    dump_fd, dump_path, tmp_output_dir)
        end

        --options.split_to_chunks(
        --        lex_ctx, dump_fd, dump_path, tmp_output_dir, hooks_ctx, options)

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

        e.os.rename(tmp_output_dir, output_dir)

        if hooks_ctx.renamed_output_dir_handler then
            hooks_ctx:renamed_output_dir_handler(tmp_output_dir, output_dir)
        end
        if hooks_ctx.end_program_handler then
            hooks_ctx:end_program_handler()
        end
    end, e.debug.traceback)

    if dump_fd then dump_fd:close() end
    if lex_ctx then lex_ctx:free() end

    e.assert(res, err)
end

return {
    make_default_options = make_default_options,
    pg_dump_splitter = pg_dump_splitter,
}
