function(testprogram name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} ${PROJECT_NAME})
    target_compile_options(${name} PRIVATE -Wno-unused-parameter)
endfunction()

testprogram(jitterentropy-rng)