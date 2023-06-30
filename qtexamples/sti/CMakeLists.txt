project(sti)

aux_source_directory(. SRCS)
add_library(sti STATIC ${SRCS})
