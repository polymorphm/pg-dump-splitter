local std, _ENV = _ENV

local export = {}

function export.make_options_from_pg_dump_splitter(options)
  return {
    io_size = options.io_size,
    open = options.open,
    mkdir = options.mkdir,
    remove = options.remove,
    ident_str_to_file_str = options.ident_str_to_file_str,
    no_schema_dirs = options.no_schema_dirs,
    relaxed_order = options.relaxed_order,
    sql_footer = options.sql_footer,
  }
end

function export.regular_rule_handler(rule, obj_type, obj_values)
  local directories, filename
  local obj_title = rule.args[1]

  std.assert(obj_values.obj_name, 'no object name')

  if obj_values.obj_schema then
    if rule.no_schema_dirs then
      directories = {obj_title}
      filename = obj_values.obj_schema .. '.' .. obj_values.obj_name
    else
      directories = {obj_values.obj_schema, obj_title}
      filename = obj_values.obj_name
    end
  else
    directories = {obj_title}
    filename = obj_values.obj_name
  end

  return directories, filename, rule.order
end

function export.relative_rule_handler(rule, obj_type, obj_values)
  local directories, filename
  local rel_title = rule.args[1]

  std.assert(obj_values.rel_name, 'no relative name')

  if obj_values.rel_schema then
    if rule.no_schema_dirs then
      directories = {rel_title}
      filename = obj_values.rel_schema .. '.' .. obj_values.rel_name
    else
      directories = {obj_values.rel_schema, rel_title}
      filename = obj_values.rel_name
    end
  else
    directories = {rel_title}
    filename = obj_values.rel_name
  end

  return directories, filename, rule.order
end

function export.rude_rule_handler(rule, obj_type, obj_values)
  local obj_title = rule.args[1]

  return {}, obj_title, rule.order
end

function export.schema_rule_handler(rule, obj_type, obj_values)
  local directories, filename
  local obj_title = rule.args[1]

  std.assert(obj_values.obj_name, 'no object name')

  if rule.no_schema_dirs then
    directories = {obj_title}
    filename = obj_values.obj_name
  else
    directories = {obj_values.obj_name}
    filename = obj_title
  end

  return directories, filename, rule.order
end

function export.make_sort_rules(options)
  local reg = export.regular_rule_handler
  local rel = export.relative_rule_handler
  local rude = export.rude_rule_handler
  local schema = export.schema_rule_handler

  local items = {
    {'set', rude, 'MISC'},
    {'select', rude, 'MISC'},
    {'unprocessed', rude, 'OTHER'},

    {'create_schema', schema, 'SCHEMA'},
    {'create_extension', reg, 'EXTENSION'},
    {'create_type', reg, 'TYPE'},
    {'create_function', reg, 'FUNCTION'},
    {'create_table', reg, 'TABLE'},
    {'create_sequence', reg, 'SEQUENCE'},
    {'create_cast', rude, 'CAST'},

    {'alter_schema', schema, 'SCHEMA'},
    {'alter_type', reg, 'TYPE'},
    {'alter_function', reg, 'FUNCTION'},
    {'alter_table', reg, 'TABLE'},
    {'alter_sequence', reg, 'SEQUENCE'},

    {'create_index', rel, 'TABLE'},
    {'create_trigger', rel, 'TABLE'},

    {'comment_schema', schema, 'SCHEMA'},
    {'comment_extension', reg, 'EXTENSION'},
    {'comment_type', reg, 'TYPE'},
    {'comment_function', reg, 'FUNCTION'},
    {'comment_table', reg, 'TABLE'},
    {'comment_column', reg, 'TABLE'},

    {'revoke_schema', schema, 'SCHEMA'},
    {'revoke_function', reg, 'FUNCTION'},

    {'grant_schema', schema, 'SCHEMA'},
    {'grant_function', reg, 'FUNCTION'},
  }

  local sort_rules = {}

  for i, v in std.ipairs(items) do
    sort_rules[v[1]] = {
      handler = v[2],
      args = std.table.move(v, 3, #v, 1, {}),
      order = i,
      no_schema_dirs = options.no_schema_dirs,
    }
  end

  return sort_rules
end

function export.add_to_chunk(output_dir, directories, filename, order, dump_data, options)
  local ready_path = output_dir

  for dir_i, dir in std.ipairs(directories) do
    ready_path = ready_path .. '/' .. options.ident_str_to_file_str(dir)

    options.mkdir(ready_path)
  end

  ready_path = ready_path .. '/' .. options.ident_str_to_file_str(filename) .. '.sql'
  local raw_path = ready_path .. '.chunk'
  local chunk_fd

  local ok, err = std.xpcall(function()
    chunk_fd = std.assert(options.open(raw_path, 'a'))
    local buf = ('js'):pack(order, dump_data)
    chunk_fd:write(('j'):pack(#buf), buf)
  end, std.debug.traceback)

  if chunk_fd then chunk_fd:close() end

  std.assert(ok, err)

  return raw_path, ready_path
end

function export.sort_chunk (raw_path, ready_path, options)
  local chunk_fd
  local sql_fd

  local ok, err = std.xpcall(function()
    chunk_fd = std.assert(options.open(raw_path))
    sql_fd = std.assert(options.open(ready_path, 'w'))

    local sortable = {}

    while true do
      local buf = chunk_fd:read(('j'):packsize())

      if not buf then break end

      buf = chunk_fd:read((('j'):unpack(buf)))
      local order, dump_data = ('js'):unpack(buf)

      if options.relaxed_order then
        -- relaxed order lets us write data on fly.
        -- therefore we don't insert to ``sortable``

        sql_fd:write(dump_data, '\n\n')
      else
        std.table.insert(sortable, {order, dump_data})
      end
    end

    std.table.sort(sortable, function(a, b)
      if a[1] < b[1] then return true end
      if a[1] > b[1] then return false end

      return a[2] < b[2]
    end)

    for i, v in std.ipairs(sortable) do
      sql_fd:write(v[2], '\n\n')
    end

    if options.sql_footer then
      sql_fd:write(options.sql_footer .. '\n')
    end
  end, std.debug.traceback)

  if sql_fd then sql_fd:close() end
  if chunk_fd then chunk_fd:close() end

  std.assert(ok, err)

  std.assert(options.remove(raw_path))
end

return export

-- vi:ts=2:sw=2:et
