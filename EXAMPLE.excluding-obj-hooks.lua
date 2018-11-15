local std, _ENV = _ENV

local export = {}

function export.register_hooks(hooks_ctx)
  function hooks_ctx:add_to_chunk_handler(obj_type, obj_values, directories,
      filename, order, dump_data)
    -- we will decide what miss by pattern-matching

    local obj_schema = obj_values.obj_schema or ''
    local obj_name = obj_values.obj_name or ''
    local role = obj_values.role or ''

    if obj_type == 'grant_function' and
        obj_schema:match('._taxi_.') and
        obj_name:match('get_.') and
        role:match('._client') then

      -- exclude these grants!
      -- them will not be saved to sorted dump chunks

      std.print('EXCLUDED:', obj_type,
          std.table.concat(directories, '/') .. '/' .. filename)

      return
    end

    -- others will not be ignored

    std.print('PASSED:', obj_type,
        std.table.concat(directories, '/') .. '/' .. filename)

    return directories, filename, order, dump_data
  end
end

return export

-- vi:ts=2:sw=2:et
