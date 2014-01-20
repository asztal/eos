{
  'targets' : [
    {
      'target_name' : 'eos',
      'sources' : [ 
        'src/eos.hpp', 'src/eos.cpp',
        'src/env.hpp', 'src/env.cpp',
        'src/conn.hpp', 'src/conn.cpp',
        'src/stmt.hpp', 'src/stmt.cpp',
        'src/result.hpp', 'src/result.cpp'
      ],
      'defines' : [
        'UNICODE', 'DEBUG'
      ],
      'conditions' : [
        [ 'OS == "linux"', {
          'libraries' : [ 
            '-lodbc' 
          ],
          'cflags' : [
            '-g'
          ]
        }],
        [ 'OS == "mac"', {
          'libraries' : [
            '-L/usr/local/lib',
            '-lodbc' 
          ]
        }],
        [ 'OS=="win"', {
          'libraries' : [ 
            '-lodbccp32.lib' 
          ]
        }]
      ]
    }
  ]
}
