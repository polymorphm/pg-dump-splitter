-- vim: set et ts=4 sw=4:

local e, _ENV = _ENV

local function pg_dump_splitter(dump_path, output_dir, hooks_path)
    e.print('fignya!', dump_path, output_dir, hooks_path)

end

return {
    pg_dump_splitter = pg_dump_splitter,
}
