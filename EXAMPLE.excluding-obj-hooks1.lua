local std, _ENV = _ENV

local export = {}

function export.register_hooks(hooks_ctx)
  function hooks_ctx:add_to_chunk_handler(obj_type, obj_values, directories,
      filename, order, state_keys, state_mem, dump_data)
    if directories and filename and order and state_keys and dump_data then
      -- we will decide what to miss by pattern-matching
      
      local obj_schema = obj_values.obj_schema or ''
      local obj_name = obj_values.obj_name or ''
      local role = obj_values.role or ''

      if obj_type == 'grant_function' and
          obj_schema:match('.+_taxi_.+') == obj_schema and
          obj_name:match('get_.+') == obj_name and
          role:match('.+_client') == role then

        -- exclude these grants!
        -- them will not be saved to sorted dump chunks

        std.print('EXCLUDED:', obj_type,
            std.table.concat(directories, '/') .. '/' .. filename)

        return
      end

      -- others will not be ignored

      std.print('PASSED:', obj_type,
          std.table.concat(directories, '/') .. '/' .. filename)
    end

    return directories, filename, order, state_keys, dump_data
  end
end

return export

-- vi:ts=2:sw=2:et
