-- vim: set et ts=4 sw=4:

local e, _ENV = _ENV

local function pg_dump_splitter(pump_path, output_dir, hooks_path)
    e.print("fignya!", pump_path, output_dir, hooks_path)

    return 0
end

return {
    pg_dump_splitter = pg_dump_splitter,
}
