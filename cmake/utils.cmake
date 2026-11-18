# 支持两种排除模式：
# 1. 相对路径模式：相对于 dir
# 2. 通配符模式：支持 ** 表示任意多级目录

# 使用示例：
# 1. 排除 src/gateway/grpc
# chen_set_sources(src SOURCES gateway/grpc)
#
# 2. 排除任何层级的 test 目录
# chen_set_sources(src SOURCES "**/test")
#
# 3. 排除任何以 mock 结尾的目录
# chen_set_sources(src SOURCES "**/mock")

function(chen_set_sources dir varname)
    set(exclude_patterns "")
    if(${ARGC} GREATER 2)
        foreach(i RANGE 2 ${ARGC}-1)
            list(APPEND exclude_patterns "${ARGV${i}}")
        endforeach()
    endif()
    
    chen_collect_sources(${dir} _tmp_sources "${exclude_patterns}")
    set(${varname} ${_tmp_sources} PARENT_SCOPE)
endfunction()

function(chen_collect_sources dir outputlist exclude_patterns)
    file(GLOB_RECURSE all_sources
        ${dir}/*.cc
        ${dir}/*.cpp
        ${dir}/*.c
    )
    
    if(exclude_patterns)
        foreach(pattern ${exclude_patterns})
            # 判断是否是通配符模式（包含 **）
            if(pattern MATCHES "\\*\\*")
                # 通配符模式：将 ** 替换为 .*
                string(REPLACE "**" ".*" regex_pattern "${pattern}")
            else()
                # 相对路径模式：转换为完整路径
                set(regex_pattern "${dir}/${pattern}")
            endif()
            
            # 转义路径分隔符
            string(REPLACE "/" "[/\\\\]" regex_pattern "${regex_pattern}")
            # 确保匹配完整路径
            set(regex_pattern ".*${regex_pattern}.*")
            
            # 排除匹配的文件
            list(FILTER all_sources EXCLUDE REGEX "${regex_pattern}")
        endforeach()
    endif()
    
    list(SORT all_sources)
    set(${outputlist} ${all_sources} PARENT_SCOPE)
endfunction()

# 修改__FILE__的宏
function(force_redefine_file_macro_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
        # Get source file's current list of compile definitions.
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)
        # Get the relative path of the source file in project directory
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        list(APPEND defs "__FILE__=\"${relpath}\"")
        # Set the updated compile definitions on the source file.
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs}
            )
    endforeach()
endfunction()

# ragel有限状态机
function(ragelmaker src_rl outputlist outputdir)
    #Create a custom build step that will call ragel on the provided src_rl file.
    #The output .cc file will be appended to the variable name passed in outputlist.

    get_filename_component(src_file ${src_rl} NAME_WE)

    set(rl_out ${outputdir}/${src_file}.cc)

    #adding to the list inside a function takes special care, we cannot use list(APPEND...)
    #because the results are local scope only
    set(${outputlist} ${${outputlist}} ${rl_out} PARENT_SCOPE)

    #Warning: The " -S -M -l -C -T0  --error-format=msvc" are added to match existing window invocation
    #we might want something different for mac and linux
    add_custom_command(
        OUTPUT ${rl_out}
        COMMAND cd ${outputdir}
        COMMAND ragel ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl} -o ${rl_out} -l -C -G2  --error-format=msvc
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl}
        )
    set_source_files_properties(${rl_out} PROPERTIES GENERATED TRUE)
endfunction(ragelmaker)

# 简化cmake文件
function(chen_add_executable targetname srcs depends libs)
    add_executable(${targetname} ${srcs})
    if(depends)
        add_dependencies(${targetname} ${depends})
    endif()
    force_redefine_file_macro_for_sources(${targetname})
    target_link_libraries(${targetname} ${libs})
endfunction()

# 设置目标的输出目录
# RUNTIME_OUTPUT_DIRECTORY：控制可执行文件（.exe）的输出目录
# LIBRARY_OUTPUT_DIRECTORY：控制库文件（.dll/.so）的输出目录
# ARCHIVE_OUTPUT_DIRECTORY：控制静态库（.lib/.a）的输出目录
function(chen_set_target_output_dir targetname outputdir)
    set_target_properties(${targetname} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${outputdir}"
        LIBRARY_OUTPUT_DIRECTORY "${outputdir}"
        ARCHIVE_OUTPUT_DIRECTORY "${outputdir}"
    )
endfunction()

# 封装创建服务器模块的函数
function(create_server_module module_name source_dir module_output_dir server_output_dir db_libs)
    chen_set_sources(${source_dir} MODULE_SOURCES)
    add_library(${module_name} SHARED ${MODULE_SOURCES})
    if(db_libs)
        target_link_libraries(${module_name} PRIVATE ${db_libs})
    endif()

    # 设置链接库变量
    string(TOUPPER ${module_name} MODULE_UPPER)
    set(libs
        ${CHEN_LIBRARY}
        ${CHEN_SDK_DEP_LIBS}
    )
#    message(STATUS "[DEBUG] create_server_module libs=${libs}")
    set(${MODULE_UPPER}_LIBS ${libs} PARENT_SCOPE)
    
    force_redefine_file_macro_for_sources(${module_name})
    chen_set_target_output_dir(${module_name} "${module_output_dir}")

    # 服务器程序
    # chen_add_executable(${module_name}_server "servers/${module_name}.cc" "" "${libs}")
    # chen_set_target_output_dir(${module_name}_server "${server_output_dir}")
endfunction()

# 创建独立服务模块（阶段 1：链接 blog_common 静态库）
# 参数：
#   module_name       目标名，同时作为 .so 文件名（如 user -> user.so）
#   source_dir        服务源码目录（递归收集 .cc）
#   module_output_dir 输出目录
#   common_lib        公共服务静态库目标（blog_common）
function(create_service_module module_name source_dir module_output_dir common_lib)
    # manager / logic 阶段 1 统一编入 blog_common，服务模块只编 frame + servlets
    chen_set_sources(${source_dir} MODULE_SOURCES manager logic)
    if (NOT MODULE_SOURCES)
        message(FATAL_ERROR "service ${module_name}: no sources under ${source_dir}")
    endif ()

    add_library(${module_name} SHARED ${MODULE_SOURCES})
    target_link_libraries(${module_name} PRIVATE ${common_lib})

    # 产物命名为 <service>.so，放入各服务自己的 module 目录
    set_target_properties(${module_name} PROPERTIES PREFIX "")

    force_redefine_file_macro_for_sources(${module_name})
    chen_set_target_output_dir(${module_name} "${module_output_dir}")

    if (TARGET i18n)
        add_dependencies(${module_name} i18n)
    endif ()
endfunction()
