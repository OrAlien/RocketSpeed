# Compilation flags for artifacts.
FLAGS = [
    '-std=c++11',
    '-fno-omit-frame-pointer',
    '-fexceptions',
    '-frtti',
    '-Wall',
    '-Werror',
]

FLAGS_RELEASE = FLAGS + [
    '-DNDEBUG',
    '-fvisibility=hidden'
]

FLAGS_DEBUG = FLAGS + [
    '-UNDEBUG',
    '-O0',
    '-g',
    '-ggdb',
    '-gdwarf-2',
]

# A macro which builds rocketspeed libraries in debug and release modes.
for build in [('', FLAGS_RELEASE), ('_debug', FLAGS_DEBUG)]:
    # Client C++ library.
    cxx_library(
        # name = 'rocketspeed'
        # name = 'rocketspeed_debug',
        name = 'rocketspeed' + build[0],
        # soname = 'librocketspeed.so'
        soname = 'librsclient.so',
        srcs = [
            'src/client/client.cc',
            'src/client/options.cc',
            'src/client/publisher.cc',
            'src/client/storage/file_storage.cc',
            'src/messages/descriptor_event.cc',
            'src/messages/event_loop.cc',
            'src/messages/messages.cc',
            'src/messages/msg_loop.cc',
            'src/messages/stream_socket.cc',
            'src/messages/wrapped_message.cc',
            'src/port/port_android.cc',
            'src/port/port_posix.cc',
            'src/util/build_version.cc',
            'src/util/common/base_env.cc',
            'src/util/common/client_env.cc',
            'src/util/common/coding.cc',
            'src/util/common/guid_generator.cc',
            'src/util/common/host.cc',
            'src/util/common/namespace_ids.cc',
            'src/util/common/statistics.cc',
            'src/util/common/status.cc',
            'src/util/common/thread_local.cc',
        ],
        header_namespace = '',
        exported_headers = subdir_glob([
            ('', 'include/*.h'),
            ('', 'src/**/*.h'),
            ('', 'external/folly/*.h'),
        ]),
        preprocessor_flags = [
            '-DUSE_UPSTREAM_LIBEVENT',
        ],
        exported_preprocessor_flags = [
            '-DROCKETSPEED_PLATFORM_POSIX',
            '-DOS_ANDROID',
        ],
        compiler_flags = build[1],
        deps = [
            '//native/third-party/libevent-2.0.21:libevent-2.0.21',
        ],
        visibility = [
            '//native/third-party/rocketspeed/...',
        ],
    )
    # JNI bindings to be loaded from java.
    cxx_library(
        # name = 'rocketspeed'
        # name = 'rocketspeed_debug',
        name = 'rocketspeedjni' + build[0],
        # soname = 'librocketspeedjni.so'
        soname = 'librsclientjni.so',
        srcs = glob([
            'src/djinni/*.cc',
            'src/util/common/fixed_configuration.cc',
            'src-gen/djinni/cpp/*.cpp',
            'external/djinni/support-lib/jni/djinni_support.cpp',
        ]),
        header_namespace = '',
        exported_headers = subdir_glob([
            ('', 'src-gen/djinni/**/*.hpp'),
            ('external/djinni/support-lib/jni', '*.hpp'),
            ('external/Optional', 'optional.hpp'),
        ]),
        compiler_flags = build[1],
        deps = [
            ':rocketspeed',
            '//native/third-party/android-ndk:log',
        ],
        visibility = [
            '//native/third-party/rocketspeed/...',
        ],
    )

# Rocketbench tool binary -- debug build.
cxx_binary(
    name = 'rocketbench',
    srcs = [
        'src/port/port_posix.cc',
        'src/tools/rocketbench/random_distribution.cc',
        'src/tools/rocketbench/main.cc',
        'src/util/auto_roll_logger.cc',
        'src/util/env.cc',
        'src/util/env_posix.cc',
        'src/util/parsing.cc',
        'external/gflags/src/gflags.cc',
        'external/gflags/src/gflags_completions.cc',
        'external/gflags/src/gflags_reporting.cc',
    ],
    header_namespace = '',
    headers = subdir_glob([
        ('external/gflags/include', 'gflags/*.h'),
        ('external/gflags/include/gflags', '*.h'),
    ]),
    preprocessor_flags = [
        '-DGFLAGS=gflags',
    ],
    compiler_flags = FLAGS_DEBUG,
    deps = [
        ':rocketspeed_debug',
    ],
    visibility = [
        'PUBLIC',
    ],
)

# Rocketspeed Java SDK.
java_library(
    name = 'rocketspeed-java',
    srcs = glob([
        'src/main/java/**/*.java',
        'src-gen/djinni/java/**/*.java',
    ]),
    deps = [
        ':rocketspeedjni',
    ],
    visibility = [
        '//java/com/facebook/rocketspeed/...',
    ],
)

project_config(
    src_target = ':rocketspeed-java',
    src_roots = [
        'src/main/java',
        'src-gen/djinni/java',
    ],
)
