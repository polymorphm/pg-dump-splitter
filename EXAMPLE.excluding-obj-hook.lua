local std, _ENV = _ENV

local export = {}

function export.register_hooks(hooks_ctx)
  function hooks_ctx:add_to_chunk_handler(obj_type, obj_values, directories,
      filename, order, dump_data)
    -- we will decide what miss by pattern-matching

    if obj_type == 'grant_function' and obj_values and
        obj_values.obj_schema:match('._taxi_.') and
        obj_values.obj_name:match('get_.') and
        obj_values.role:match('._client') then

      -- exclude these grants!
      -- them will not saved to sorted sump chunks

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
