local std, _ENV = _ENV

local export = {}

function export.make_options_from_pg_dump_splitter(options)
  return {
    io_size = options.io_size,
    open = options.open,
    mkdir = options.mkdir,
    ident_str_to_file_str = options.ident_str_to_file_str,
    no_schema_dirs = options.no_schema_dirs,
    relaxed_order = options.relaxed_order,
  }
end

function export.regular_rule_handler(rule, obj_type, obj_values)
  local directories, filename
  local obj_title = rule.args[1]

  if obj_values.obj_schema and obj_values.obj_name then
    if rule.no_schema_dirs then
      directories = {obj_title}
      filename = obj_values.obj_name
    else
      directories = {obj_values.obj_schema, obj_title}
      filename = obj_values.obj_name
    end
  elseif obj_values.obj_name_wo_schema then
    directories = {obj_title}
    filename = obj_values.obj_name_wo_schema
  else
    std.error('no object schema/name')
  end

  return directories, filename, rule.order
end

function export.relative_rule_handler(rule, obj_type, obj_values)
  local directories, filename
  local rel_title = rule.args[1]

  if obj_values.rel_schema and obj_values.rel_name then
    if rule.no_schema_dirs then
      directories = {rel_title}
      filename = obj_values.rel_name
    else
      directories = {obj_values.rel_schema, rel_title}
      filename = obj_values.rel_name
    end
  elseif obj_values.rel_name_wo_schema then
    directories = {rel_title}
    filename = obj_values.rel_name_wo_schema
  else
    std.error('no relative schema/name')
  end

  return directories, filename, rule.order
end

function export.rude_rule_handler(rule, obj_type, obj_values)
  local obj_title = rule.args[1]

  return {}, obj_title, rule.order
end

function export.make_sort_rules(options)
  local reg = export.regular_rule_handler
  local rel = export.relative_rule_handler
  local rude = export.rude_rule_handler

  local items = {
    {'create_function', reg, 'FUNCTION'},
    {'alter_function_owner', reg, 'FUNCTION'},

    -- TODO ... ... ...
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
    -- TODO ... ... ...
  end, std.debug.traceback)

  if chunk_fd then chunk_fd:close() end

  std.assert(ok, err)

  return raw_path, ready_path
end

function export.sort_chunk (raw_path, ready_path, options)
  -- TODO ... ...
end

return export

-- vi:ts=2:sw=2:et
