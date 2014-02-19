{
  'targets' : [
    {
      'target_name' : 'eos',
      'sources' : [ 
        'src/handle.hpp', 'src/handle.cpp',
        'src/eos.hpp', 'src/eos.cpp',
        'src/env.hpp', 'src/env.cpp',
        'src/conn.hpp', 'src/conn.cpp',
          'src/conn.connect.cpp',
          'src/conn.disconnect.cpp',
          'src/conn.browseConnect.cpp',
        'src/stmt.hpp', 'src/stmt.cpp',
          'src/stmt.describeCol.cpp',
          'src/stmt.execDirect.cpp',
          'src/stmt.execute.cpp',
          'src/stmt.fetch.cpp',
          'src/stmt.getData.cpp',
          'src/stmt.numResultCols.cpp',
          'src/stmt.prepare.cpp',
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
