local std, _ENV = _ENV

local export = {}

function export.make_pattern_rules(handlers)
  local kw = handlers.kw_rule_handler
  local ss = handlers.ss_rule_handler
  local ident = handlers.ident_rule_handler
  local str = handlers.str_rule_handler
  local op_ident = handlers.op_ident_rule_handler
  local en = handlers.en_rule_handler
  local any =  handlers.any_rule_handler
  local fork = handlers.fork_rule_handler
  local rep = handlers.rep_rule_handler

  return {
    {
      'set',
      {kw, 'set'},
      {any},
      {en},
    },

    {
      'select',
      {kw, 'select'},
      {any},
      {en},
    },

    {
      'comment_database',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'database'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_schema',
      {kw, 'create'},
      {kw, 'schema'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_schema',
      {kw, 'alter'},
      {kw, 'schema'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_schema',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'schema'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'revoke_schema',
      {kw, 'revoke'},
      {any},
      {kw, 'on'},
      {kw, 'schema'},
      {ident, 'obj_name'},
      {any},
      {kw, 'from'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'grant_schema',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {kw, 'schema'},
      {ident, 'obj_name'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'alter_default_privileges_revoke',
      {kw, 'alter'},
      {kw, 'default'},
      {kw, 'privileges'},
      {kw, 'for'},
      {
        fork,
        {
          {kw, 'role'},
        },
        {
          {kw, 'user'},
        },
      },
      {ident, 'role'},
      {any},
      {kw, 'revoke'},
      {any},
      {en},
    },

    {
      'alter_default_privileges_grant',
      {kw, 'alter'},
      {kw, 'default'},
      {kw, 'privileges'},
      {kw, 'for'},
      {
        fork,
        {
          {kw, 'role'},
        },
        {
          {kw, 'user'},
        },
      },
      {ident, 'role'},
      {any},
      {kw, 'grant'},
      {any},
      {en},
    },

    {
      'create_extension',
      {kw, 'create'},
      {kw, 'extension'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_extension',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'extension'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_type',
      {kw, 'create'},
      {kw, 'type'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'as'},
      {any},
      {en},
    },

    {
      'alter_type',
      {kw, 'alter'},
      {kw, 'type'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_type',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'type'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_domain',
      {kw, 'create'},
      {kw, 'domain'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_domain',
      {kw, 'alter'},
      {kw, 'domain'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_domain',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'domain'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_function',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'or'},
          {kw, 'replace'},
        },
        {},
      },
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'alter_function',
      {kw, 'alter'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'comment_function',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'revoke_function',
      {kw, 'revoke'},
      {any},
      {kw, 'on'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {kw, 'from'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'grant_function',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {kw, 'function'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'create_table',
      {kw, 'create'},
      {
        rep,
        {
          fork,
          {
            {kw, 'global'},
          },
          {
            {kw, 'local'},
          },
          {
            {kw, 'temporary'},
          },
          {
            {kw, 'temp'},
          },
          {
            {kw, 'unlogged'},
          },
          {
            {kw, 'foreign'},
          },
        },
      },
      {kw, 'table'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {
        fork,
        {
          {kw, 'partition'},
          {kw, 'of'},
          {
            fork,
            {
              {ident, 'parent_schema'},
              {ss, '.'},
            },
            {},
          },
          {ident, 'parent_name'},
        },
        {
          {kw, 'of'},
          {
            fork,
            {
              {ident, 'type_schema'},
              {ss, '.'},
            },
            {},
          },
          {ident, 'type_name'},
        },
        {
          {ss, '('},
          {any},
          {ss, ')'},
        },
      },
      {any},
      {en},
    },

    {
      'alter_table',
      {kw, 'alter'},
      {
        fork,
        {
          {kw, 'foreign'},
        },
        {},
      },
      {kw, 'table'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'exists'},
        },
        {},
      },
      {
        fork,
        {
          {kw, 'only'},
        },
        {},
      },
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_table',
      {kw, 'comment'},
      {kw, 'on'},
      {
        fork,
        {
          {kw, 'foreign'},
        },
        {},
      },
      {kw, 'table'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'comment_column',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'column'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '.'},
      {ident, 'col_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'revoke_table',
      {kw, 'revoke'},
      {any},
      {kw, 'on'},
      {
        fork,
        {
          {kw, 'foreign'},
        },
        {},
      },
      {kw, 'table'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {kw, 'from'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'grant_table',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {
        fork,
        {
          {kw, 'foreign'},
        },
        {},
      },
      {kw, 'table'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'create_rule',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'or'},
          {kw, 'replace'},
        },
        {},
      },
      {kw, 'rule'},
      {ident, 'obj_name'},
      {kw, 'as'},
      {any},
      {kw, 'to'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {
        fork,
        {
          {kw, 'where'},
          {any},
        },
        {},
      },
      {kw, 'do'},
      {any},
      {en},
    },

    {
      'comment_rule',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'rule'},
      {ident, 'obj_name'},
      {kw, 'on'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_view',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'or'},
          {kw, 'replace'},
        },
        {},
      },
      {
        rep,
        {
          fork,
          {
            {kw, 'temporary'},
          },
          {
            {kw, 'temp'},
          },
          {
            {kw, 'recursive'},
          },
        },
      },
      {kw, 'view'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {
        fork,
        {
          {ss, '('},
          {any},
          {ss, ')'},
        },
        {},
      },
      {
        fork,
        {
          {kw, 'with'},
          {ss, '('},
          {any},
          {ss, ')'},
        },
        {},
      },
      {kw, 'as'},
      {any},
      {en},
    },

    {
      'comment_view',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'view'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_sequence',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'temporary'}
        },
        {
          {kw, 'temp'}
        },
        {},
      },
      {kw, 'sequence'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_sequence',
      {kw, 'alter'},
      {kw, 'sequence'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'exists'},
        },
        {},
      },
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_sequence',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'sequence'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'revoke_sequence',
      {kw, 'revoke'},
      {any},
      {kw, 'on'},
      {kw, 'sequence'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {kw, 'from'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'grant_sequence',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {kw, 'sequence'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'create_index',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'unique'},
        },
        {},
      },
      {kw, 'index'},
      {
        fork,
        {
          {kw, 'concurrently'},
        },
        {},
      },
      {
        fork,
        {
          {
            fork,
            {
              {kw, 'if'},
              {kw, 'not'},
              {kw, 'exists'},
            },
            {},
          },
          {ident, 'obj_name'},
        },
        {},
      },
      {kw, 'on'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {any},
      {en},
    },

    {
      'comment_index',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'index'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'comment_constraint',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'constraint'},
      {ident, 'obj_name'},
      {kw, 'on'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'comment_constraint_domain',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'constraint'},
      {ident, 'obj_name'},
      {kw, 'on'},
      {kw, 'domain'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_trigger',
      {kw, 'create'},
      {kw, 'trigger'},
      {ident, 'obj_name'},
      {any},
      {kw, 'on'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {any},
      {en},
    },

    {
      'comment_trigger',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'trigger'},
      {ident, 'obj_name'},
      {kw, 'on'},
      {
        fork,
        {
          {ident, 'rel_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'rel_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_cast',
      {kw, 'create'},
      {kw, 'cast'},
      {any},
      {en},
    },

    {
      'comment_cast',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'cast'},
      {ss, '('},
      {any},
      {ss, ')'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_event_trigger',
      {kw, 'create'},
      {kw, 'event'},
      {kw, 'trigger'},
      {ident, 'obj_name'},
      {kw, 'on'},
      {any},
      {en},
    },

    {
      'alter_event_trigger',
      {kw, 'alter'},
      {kw, 'event'},
      {kw, 'trigger'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_event_trigger',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'event'},
      {kw, 'trigger'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_operator',
      {kw, 'create'},
      {kw, 'operator'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {op_ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'alter_operator',
      {kw, 'alter'},
      {kw, 'operator'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {op_ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'comment_operator',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'operator'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {op_ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_aggregate',
      {kw, 'create'},
      {kw, 'aggregate'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'alter_aggregate',
      {kw, 'alter'},
      {kw, 'aggregate'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {any},
      {en},
    },

    {
      'comment_aggregate',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'aggregate'},
      {
        fork,
        {
          {ident, 'obj_schema'},
          {ss, '.'},
        },
        {},
      },
      {ident, 'obj_name'},
      {ss, '('},
      {any},
      {ss, ')'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_language',
      {kw, 'create'},
      {
        fork,
        {
          {kw, 'or'},
          {kw, 'replace'},
        },
        {},
      },
      {
        rep,
        {
          fork,
          {
            {kw, 'trusted'},
          },
          {
            {kw, 'procedural'},
          },
        },
      },
      {kw, 'language'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_language',
      {kw, 'alter'},
      {
        fork,
        {
          {kw, 'procedural'},
        },
        {},
      },
      {kw, 'language'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_language',
      {kw, 'comment'},
      {kw, 'on'},
      {
        fork,
        {
          {kw, 'procedural'},
        },
        {},
      },
      {kw, 'language'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_foreign_data_wrapper',
      {kw, 'create'},
      {kw, 'foreign'},
      {kw, 'data'},
      {kw, 'wrapper'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_foreign_data_wrapper',
      {kw, 'alter'},
      {kw, 'foreign'},
      {kw, 'data'},
      {kw, 'wrapper'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_foreign_data_wrapper',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'foreign'},
      {kw, 'data'},
      {kw, 'wrapper'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'create_server',
      {kw, 'create'},
      {kw, 'server'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'alter_server',
      {kw, 'alter'},
      {kw, 'server'},
      {ident, 'obj_name'},
      {any},
      {en},
    },

    {
      'comment_server',
      {kw, 'comment'},
      {kw, 'on'},
      {kw, 'server'},
      {ident, 'obj_name'},
      {kw, 'is'},
      {str, 'comment'},
      {en},
    },

    {
      'revoke_server',
      {kw, 'revoke'},
      {any},
      {kw, 'on'},
      {kw, 'foreign'},
      {kw, 'server'},
      {ident, 'obj_name'},
      {any},
      {kw, 'from'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'grant_server',
      {kw, 'grant'},
      {any},
      {kw, 'on'},
      {kw, 'foreign'},
      {kw, 'server'},
      {ident, 'obj_name'},
      {any},
      {kw, 'to'},
      {ident, 'role'},
      {any},
      {en},
    },

    {
      'create_user_mapping',
      {kw, 'create'},
      {kw, 'user'},
      {kw, 'mapping'},
      {
        fork,
        {
          {kw, 'if'},
          {kw, 'not'},
          {kw, 'exists'},
        },
        {},
      },
      {kw, 'for'},
      {ident, 'role'},
      {kw, 'server'},
      {ident, 'obj_name'},
      {any},
      {en},
    },
  }
end

return export

-- vi:ts=2:sw=2:et
