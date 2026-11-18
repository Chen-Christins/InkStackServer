# 阶段 1：公共静态库
# 说明：各域 manager / logic 暂时全部编入 blog_common，供四个服务共同链接；
#      阶段 2 引入 RPC 后再按服务收回。

set(BLOG_COMMON_SOURCES)

chen_collect_sources(${CMAKE_CURRENT_SOURCE_DIR} COMMON_SOURCES "")
list(APPEND BLOG_COMMON_SOURCES ${COMMON_SOURCES})

foreach (domain user article comment platform)
    set(DOMAIN_MANAGER_SOURCES)
    set(DOMAIN_LOGIC_SOURCES)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../${domain}/manager)
        chen_collect_sources(${CMAKE_CURRENT_SOURCE_DIR}/../${domain}/manager DOMAIN_MANAGER_SOURCES "")
    endif ()
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../${domain}/logic)
        chen_collect_sources(${CMAKE_CURRENT_SOURCE_DIR}/../${domain}/logic DOMAIN_LOGIC_SOURCES "")
    endif ()
    list(APPEND BLOG_COMMON_SOURCES ${DOMAIN_MANAGER_SOURCES} ${DOMAIN_LOGIC_SOURCES})
endforeach ()

add_library(blog_common STATIC ${BLOG_COMMON_SOURCES})
force_redefine_file_macro_for_sources(blog_common)
add_dependencies(blog_common dbproxy_data)

# 与改造前的 blog 模块保持一致：只传递 dbproxy_data。
# libchen.so / 三方库的符号由运行时主程序（bin/server，-rdynamic）提供，
# 这里显式链接会把 deps/lib 下的静态库（protobuf/jsoncpp 等）打进模块，
# 与 libchen.so 内的同名符号重复，反而引入隐患。
target_link_libraries(blog_common PUBLIC dbproxy_data)

target_include_directories(blog_common PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/protocol/include
)
