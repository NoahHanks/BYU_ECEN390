add_executable(lasertag.elf
main.c
queue_test.c
filter.c
filterTest.c
histogram.c
isr.c
trigger.c
transmitter.c
hitLedTimer.c
lockoutTimer.c
detector.c
sound.c
timer_ps.c
runningModes.c
runningModes2.c
)

add_subdirectory(sounds)
#add_subdirectory(bluetooth) # Optional code for the creative project.
target_link_libraries(lasertag.elf ${330_LIBS} sounds lasertag queue)
set_target_properties(lasertag.elf PROPERTIES LINKER_LANGUAGE CXX)
