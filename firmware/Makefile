#   The following are locations and directory names.  Change these to fit
#   your needs.  The output .elf file winds up in the $BINDIR directory.
#   Use your own programmer.  I used the OpenOCD software with an STLINK V2
#   USB programmer.
#

GCC_PREFIX=arm-none-eabi
GCC_SIZE=arm-none-eabi-size
OCM3DIR=../libopencm3
MCU=stm32-f407
LDIR=$(OCM3DIR)/lib

# Standard locations of source, object, include, binary and dependent files.

SRCDIR:=./src
OBJDIR:=./obj
INCDIR:=./inc
BINDIR:=./bin

#   Files.

SRCS:= main.c cli.c dbserial.c sdiosubs.c uart.c \
 comm.c diskio.c ffunicode.c miscsubs.c tapedriver.c usbcdc.c \
 crc16.c ff.c filesub.c rtcsubs.c tapeutil.c ymodem.c
OBJS:= $(addprefix $(OBJDIR)/,$(SRCS:.c=.o)) 
SRCS:= $(addprefix $(SRCDIR)/,$(SRCS))

#   Flags and definitions.

TARGET=target.elf
MAP=target.map
CC=$(GCC_PREFIX)-gcc
DEVICE=STM32F4
FLOAT_OPT=-mfloat-abi=hard -mfpu=fpv4-sp-d16
F4DEFINES=
COMPILE_INC=-I$(OCM3DIR)/include -Iinc
LINK_DIR=-L$(OCM3DIR)/lib

.PHONY: $(DEPDIR)/%.d

GCC_OPT=-Os -std=c99 -g -mthumb -mcpu=cortex-m4  $(FLOAT_OPT) \
-Wextra -Wshadow -Wimplicit-function-declaration $(F4DEFINES) \
-Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes -fno-common \
-ffunction-sections -fdata-sections  -Wall -Wundef -D$(DEVICE) 

GCC_LINK_OPT1=--static -nostartfiles -mthumb -mcpu=cortex-m4 \
-T ./$(MCU).ld  -Wl,-Map=$(MAP) -Wl,--gc-sections $(FLOAT_OPT) \
$(LINK_DIR) 
GCC_LINK_OPT2=-lopencm3_stm32f4 -Wl,--start-group -lc -lgcc -lnosys \
-Wl,--end-group 

#  Create result directories, if they don't exist already

$(shell mkdir -p $(OBJDIR) >/dev/null)
$(shell mkdir -p $(BINDIR) >/dev/null)

DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

all: $(BINDIR)/$(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c 
	$(CC) $(GCC_OPT) $(COMPILE_INC)  -c  -o $@ $<

$(BINDIR)/$(TARGET): $(OBJS)
	$(CC) $(GCC_LINK_OPT1) $(OBJS) $(GCC_LINK_OPT2)  -o $@
	$(GCC_SIZE) $@

.PHONY: clean	

clean:
	rm -f $(BINDIR)/* $(OBJDIR)/* $(MAP)
	
