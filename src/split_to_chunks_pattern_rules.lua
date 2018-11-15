local std, _ENV = _ENV

local export = {}

function export.make_pattern_rules(handlers)
  local kw = handlers.kw_rule_handler
  local ss = handlers.ss_rule_handler
  local ident = handlers.ident_rule_handler
  local str = handlers.str_rule_handler
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
      'create_cast',
      {kw, 'create'},
      {kw, 'cast'},
      {any},
      {en},
    },
  }
end

return export

-- vi:ts=2:sw=2:et
