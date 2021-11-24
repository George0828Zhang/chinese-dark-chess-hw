# object file folder
OBJ_FOLDER := bin

# check OS
ifeq ($(OS), Windows_NT) # windows
	G_PLUS_PLUS := g++ -std=c++11 -static -O3 -Wall -g
	EXEC := hw2.exe
	DELETE := del /f $(OBJ_FOLDER)\*.o $(EXEC)
	MAKE_FOLDER := if not exist $(OBJ_FOLDER) mkdir $(OBJ_FOLDER)
	OS_DEFINE := -DWINDOWS
else
	ifeq ($(shell uname), Darwin) # macOS
		G_PLUS_PLUS := g++ -std=c++11 -O3 -Wall -g
		OS_DEFINE := -DMACOS
	else # linux
		G_PLUS_PLUS := g++ -std=c++11 -static -O3 -Wall -g
		OS_DEFINE := -DLINUX
	endif
	EXEC := exec.hw2
	DELETE := rm -f $(OBJ_FOLDER)/*.o $(EXEC)
	MAKE_FOLDER := mkdir -p $(OBJ_FOLDER)
endif

.PHONY: all
all : hw2

# specify files
MAIN := main
MAIN_FILE := $(MAIN).cpp
MAIN_OBJ := $(OBJ_FOLDER)/$(MAIN).o

AI := MyAI
AI_FILE := $(AI).cpp
AI_HEADER := $(AI).h
AI_OBJ := $(OBJ_FOLDER)/$(AI).o

PRNG := pcg_basic
PRNG_FILE := $(PRNG).c
PRNG_HEADER := $(PRNG).h
PRNG_OBJ := $(OBJ_FOLDER)/$(PRNG).o

# command
$(AI_OBJ) : $(AI_FILE) $(AI_HEADER) 
	$(MAKE_FOLDER)
	$(G_PLUS_PLUS) -c $(AI_FILE) $(OS_DEFINE) -o $(AI_OBJ)

$(PRNG_OBJ) : $(PRNG_FILE) $(PRNG_HEADER) 
	$(MAKE_FOLDER)
	$(G_PLUS_PLUS) -c $(PRNG_FILE) -o $(PRNG_OBJ)

$(EXEC) : $(AI_OBJ) $(PRNG_OBJ) $(MAIN_FILE)
	$(MAKE_FOLDER)
	$(G_PLUS_PLUS) -c $(MAIN_FILE) $(OS_DEFINE) -o $(MAIN_OBJ)
	$(G_PLUS_PLUS) $(AI_OBJ) $(PRNG_OBJ) $(MAIN_OBJ) -o $(EXEC)

# target
hw2 : $(EXEC)

# clean object files and executable file
.PHONY: clean
clean: 
	$(DELETE)