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
    split_stateless = options.split_stateless,
    relaxed_order = options.relaxed_order,
    sql_footer = options.sql_footer,
  }
end

export.sort_rule_proto = {}

function export.sort_rule_proto:get_state_keys_from_args(rule_arg_i)
  -- this helper-function avoids returning nil, and it looks to options

  if self.split_stateless then
    return {}
  end

  return self.args[rule_arg_i] or {}
end

function export.set_rule_handler(rule, obj_type, obj_values,
    state_mem, dump_data)
  local set_key_set = rule.args[1]

  if not rule.split_stateless and set_key_set[obj_values.obj_name] then
    state_mem[obj_values.obj_name] = dump_data
    return
  end

  local obj_title = rule.args[2]
  local state_keys = rule:get_state_keys_from_args(3)

  return {}, obj_title, rule.order, state_keys
end

function export.regular_rule_handler(rule, obj_type, obj_values,
    state_mem, dump_data)
  local directories, filename
  local obj_title = rule.args[1]
  local state_keys = rule:get_state_keys_from_args(2)

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

  return directories, filename, rule.order, state_keys
end

function export.relative_rule_handler(rule, obj_type, obj_values,
    state_mem, dump_data)
  local directories, filename
  local rel_title = rule.args[1]
  local state_keys = rule:get_state_keys_from_args(2)

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

  return directories, filename, rule.order, state_keys
end

function export.rude_rule_handler(rule, obj_type, obj_values,
    state_mem, dump_data)
  local obj_title = rule.args[1]
  local state_keys = rule:get_state_keys_from_args(2)

  return {}, obj_title, rule.order, state_keys
end

function export.schema_rule_handler(rule, obj_type, obj_values,
    state_mem, dump_data)
  local directories, filename
  local obj_title = rule.args[1]
  local state_keys = rule:get_state_keys_from_args(2)

  std.assert(obj_values.obj_name, 'no object name')

  if rule.no_schema_dirs then
    directories = {obj_title}
    filename = obj_values.obj_name
  else
    directories = {obj_values.obj_name}
    filename = obj_title
  end

  return directories, filename, rule.order, state_keys
end

function export.ugly_rule_handler(rule, obj_type, obj_values,
    state_mem, dump_data)
  -- it's just almost regular handler, but
  -- its objects have ugly names (or without names at all, but with schames?).
  -- we don't want to save them with their ugly names

  local directories, filename
  local obj_title = rule.args[1]
  local state_keys = rule:get_state_keys_from_args(2)

  if obj_values.obj_schema then
    if rule.no_schema_dirs then
      directories = {obj_title}
      filename = obj_values.obj_schema
    else
      directories = {obj_values.obj_schema}
      filename = obj_title
    end
  else
    directories = {}
    filename = obj_title
  end

  return directories, filename, rule.order, state_keys
end

function export.make_sort_rules(options)
  local set = export.set_rule_handler
  local reg = export.regular_rule_handler
  local rel = export.relative_rule_handler
  local rude = export.rude_rule_handler
  local schema = export.schema_rule_handler
  local ugly = export.ugly_rule_handler

  local set_keys = {'default_tablespace', 'default_with_oids'}

  local set_key_set = set_keys
  for i, v in std.ipairs(set_keys) do set_key_set[v] = true end

  local items = {
    {'set', set, set_key_set, 'MISC'},
    {'select', rude, 'MISC'},
    {'unprocessed', rude, 'OTHER'},

    {'create_schema', schema, 'SCHEMA'},
    {'create_extension', reg, 'EXTENSION'},
    {'create_type', reg, 'TYPE'},
    {'create_domain', reg, 'DOMAIN'},
    {'create_function', reg, 'FUNCTION'},
    {'create_table', reg, 'TABLE', {
      'default_tablespace',
      'default_with_oids',
    }},
    {'create_view', reg, 'TABLE'},
    {'create_sequence', reg, 'TABLE'},
    {'create_cast', rude, 'CAST'},
    {'create_event_trigger', reg, 'EVENT_TRIGGER'},
    {'create_operator', ugly, 'OPERATOR'},
    {'create_aggregate', reg, 'AGGREGATE'},
    {'create_language', reg, 'LANGUAGE'},
    {'create_foreign_data_wrapper', reg, 'FOREIGN_DATA_WRAPPER'},
    {'create_server', reg, 'SERVER'},
    {'create_user_mapping', reg, 'USER_MAPPING'},

    {'alter_default_privileges_revoke', rude, 'DEFAULT_PRIVILEGES'},
    {'alter_default_privileges_grant', rude, 'DEFAULT_PRIVILEGES'},

    {'alter_schema', schema, 'SCHEMA'},
    {'alter_type', reg, 'TYPE'},
    {'alter_domain', reg, 'DOMAIN'},
    {'alter_function', reg, 'FUNCTION'},
    {'alter_table', reg, 'TABLE'},
    {'alter_sequence', reg, 'TABLE'},
    {'alter_event_trigger', reg, 'EVENT_TRIGGER'},
    {'alter_operator', ugly, 'OPERATOR'},
    {'alter_aggregate', reg, 'AGGREGATE'},
    {'alter_language', reg, 'LANGUAGE'},
    {'alter_foreign_data_wrapper', reg, 'FOREIGN_DATA_WRAPPER'},
    {'alter_server', reg, 'SERVER'},

    {'create_rule', rel, 'TABLE'},
    {'create_index', rel, 'TABLE', {
      'default_tablespace',
    }},
    {'create_trigger', rel, 'TABLE'},

    {'comment_database', rude, 'DATABASE'},
    {'comment_schema', schema, 'SCHEMA'},
    {'comment_extension', reg, 'EXTENSION'},
    {'comment_type', reg, 'TYPE'},
    {'comment_domain', reg, 'DOMAIN'},
    {'comment_function', reg, 'FUNCTION'},
    {'comment_table', reg, 'TABLE'},
    {'comment_rule', rel, 'TABLE'},
    {'comment_view', reg, 'TABLE'},
    {'comment_column', reg, 'TABLE'},
    {'comment_sequence', reg, 'TABLE'},
    {'comment_cast', rude, 'CAST'},
    {'comment_index', reg, 'TABLE'},
    {'comment_constraint', rel, 'TABLE'},
    {'comment_constraint_domain', rel, 'DOMAIN'},
    {'comment_trigger', rel, 'TABLE'},
    {'comment_event_trigger', reg, 'EVENT_TRIGGER'},
    {'comment_operator', ugly, 'OPERATOR'},
    {'comment_aggregate', reg, 'AGGREGATE'},
    {'comment_language', reg, 'LANGUAGE'},
    {'comment_foreign_data_wrapper', reg, 'FOREIGN_DATA_WRAPPER'},
    {'comment_server', reg, 'SERVER'},

    {'revoke_schema', schema, 'SCHEMA'},
    {'revoke_function', reg, 'FUNCTION'},
    {'revoke_table', reg, 'TABLE'},
    {'revoke_sequence', reg, 'TABLE'},
    {'revoke_server', reg, 'SERVER'},

    {'grant_schema', schema, 'SCHEMA'},
    {'grant_function', reg, 'FUNCTION'},
    {'grant_table', reg, 'TABLE'},
    {'grant_sequence', reg, 'TABLE'},
    {'grant_server', reg, 'SERVER'},
  }

  local sort_rules = {}

  for i, v in std.ipairs(items) do
    sort_rules[v[1]] = std.setmetatable(
      {
        handler = v[2],
        args = std.table.move(v, 3, #v, 1, {}),
        order = i,
        no_schema_dirs = options.no_schema_dirs,
        split_stateless = options.split_stateless,
      },
      {__index = export.sort_rule_proto}
    )
  end

  return sort_rules
end

function export.add_to_chunk(output_dir, directories, filename, order,
    state_keys, state_mem, dump_data, options)
  local ready_path = output_dir

  for dir_i, dir in std.ipairs(directories) do
    ready_path = ready_path .. '/' .. options.ident_str_to_file_str(dir)

    options.mkdir(ready_path)
  end

  ready_path = ready_path .. '/' ..
      options.ident_str_to_file_str(filename) .. '.sql'
  local raw_path = ready_path .. '.chunk'
  local state_keyvalues = {}
  local state_values = {}

  for i, state_key in std.ipairs(state_keys) do
    local state_value = state_mem[state_key]

    if state_value then
      std.table.insert(state_keyvalues, state_key)
      std.table.insert(state_values, state_value)
    end
  end
  std.table.move(state_values, 1, #state_values, #state_keyvalues + 1,
      state_keyvalues)

  local chunk_fd

  local ok, err = std.xpcall(function()
    chunk_fd = std.assert(options.open(raw_path, 'ab'))
    local buf = ('jsj' .. ('s'):rep(#state_keyvalues)):pack(order, dump_data,
        #state_keyvalues, std.table.unpack(state_keyvalues))
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
    chunk_fd = std.assert(options.open(raw_path, 'rb'))
    sql_fd = std.assert(options.open(ready_path, 'wb'))

    local sortable = {}
    local written_state_mem = {}

    local function write_state_value_if_needed(state_keys, state_values,
        write_func)
      for i, state_key in std.ipairs(state_keys) do
        local state_value = state_values[i]

        if written_state_mem[state_key] ~= state_value then
          write_func(state_key, state_value)
          written_state_mem[state_key] = state_value
        end
      end
    end

    while true do
      local buf = chunk_fd:read(('j'):packsize())

      if not buf then break end

      buf = chunk_fd:read((('j'):unpack(buf)))
      local order, dump_data, state_value_count, n = ('jsj'):unpack(buf)
      local state_keyvalues = std.table.move(
          std.table.pack(('s'):rep(state_value_count):unpack(buf, n)),
          1, state_value_count, 1, {})
      local state_keys = std.table.move(state_keyvalues,
          1, #state_keyvalues / 2, 1, {})
      local state_values = std.table.move(state_keyvalues,
          #state_keys + 1, #state_keyvalues, 1, {})

      if options.relaxed_order then
        -- relaxed order lets us write data on fly.
        -- therefore we don't insert to ``sortable``

        write_state_value_if_needed(state_keys, state_values,
            function(state_key, state_value)
              sql_fd:write(state_value, '\n\n')
            end)
        sql_fd:write(dump_data, '\n\n')
      else
        std.table.insert(sortable,
            {order, dump_data, state_keys, state_values})
      end
    end

    std.table.sort(sortable, function(a, b)
      if a[1] < b[1] then return true end
      if a[1] > b[1] then return false end

      return a[2] < b[2]
    end)

    for i, v in std.ipairs(sortable) do
      local dump_data = v[2]
      local state_keys = v[3]
      local state_values = v[4]

      write_state_value_if_needed(state_keys, state_values,
          function(state_key, state_value)
            sql_fd:write(state_value, '\n\n')
          end)
      sql_fd:write(dump_data, '\n\n')
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
