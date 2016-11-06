################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/camera.cpp \
../src/opencv_test.cpp 

OBJS += \
./src/camera.o \
./src/opencv_test.o 

CPP_DEPS += \
./src/camera.d \
./src/opencv_test.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-angstrom-linux-gnueabi-g++ -I/home/sumir/openembedded/oe-core/build/out-glibc/sysroots/colibri-t20/usr/include -I/home/sumir/openembedded/oe-core/build/out-glibc/sysroots/colibri-t20/usr/include/glib-2.0 -I/home/sumir/openembedded/oe-core/build/out-glibc/sysroots/colibri-t20/usr/include/gtk-2.0 -I/home/sumir/openembedded/oe-core/build/out-glibc/sysroots/colibri-t20/usr/include/gdk-pixbuf-2.0 -I/home/sumir/openembedded/oe-core/build/out-glibc/sysroots/colibri-t20/usr/include/cairo -O0 -g3 -Wall -c -fmessage-length=0 -O0 -g -Wall -lgthread-2.0 --sysroot=/home/sumir/openembedded/oe-core/build/out-glibc/sysroots/colibri-t20 -lpthread -lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_objdetect -lm   -mfloat-abi=hard -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<" `pkg-config --libs --cflags gtk+-2.0 glib-2.0 cairo` -lX11
	@echo 'Finished building: $<'
	@echo ' '


