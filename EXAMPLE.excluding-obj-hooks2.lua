local std, _ENV = _ENV

local export = {}

function export.register_hooks(hooks_ctx)
  function hooks_ctx:add_to_chunk_handler(obj_type, obj_values, directories,
        filename, order, state_keys, state_mem, dump_data)
    if directories and filename and order and state_keys and dump_data then
      -- excluding objects related with partitions.
      -- looking into ``filename`` looks like the most appropriate method.

      if filename:match('%g*_part%d+') then return end
    end

    return directories, filename, order, state_keys, dump_data
  end
end

return export

-- vi:ts=2:sw=2:et
