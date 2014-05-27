{
  'targets' : [
    {
      'target_name' : 'eos',
      'sources' : [ 
        'src/buffer.hpp', 'src/buffer.cpp',
        'src/handle.hpp', 'src/handle.cpp',
        'src/eos.hpp', 'src/eos.cpp',
        'src/env.hpp', 'src/env.cpp',
        'src/conn.hpp', 'src/conn.cpp',
          'src/conn.connect.cpp',
          'src/conn.driverConnect.cpp',
          'src/conn.disconnect.cpp',
          'src/conn.browseConnect.cpp',
        'src/operation.hpp', 'src/operation.cpp',
        'src/parameter.hpp', 'src/parameter.cpp',
        'src/stmt.hpp', 'src/stmt.cpp',
          'src/stmt.describeCol.cpp',
          'src/stmt.execDirect.cpp',
          'src/stmt.execute.cpp',
          'src/stmt.fetch.cpp',
          'src/stmt.getData.cpp',
          'src/stmt.moreResults.cpp',
          'src/stmt.numResultCols.cpp',
          'src/stmt.paramData.cpp',
          'src/stmt.prepare.cpp',
          'src/stmt.putData.cpp'
      ],
      'defines' : [
        'UNICODE', 'DEBUG'
      ],
      'include_dirs': [
        "<!(node -e \"require('nan')\")"
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
