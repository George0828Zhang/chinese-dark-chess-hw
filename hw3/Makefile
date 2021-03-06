# object file folder
OBJ_FOLDER := bin
SRC_FOLDER := src
INC_FOLDER := src/include

# check OS
ifeq ($(OS), Windows_NT) # windows
	G_PLUS_PLUS := g++ -std=gnu++11 -static -O3 -Wall -g -I $(INC_FOLDER)
	EXEC := final.exe
	DELETE := del /f $(OBJ_FOLDER)\*.o $(EXEC)
	MAKE_FOLDER := if not exist $(OBJ_FOLDER) mkdir $(OBJ_FOLDER)
	OS_DEFINE := -DWINDOWS
else
	ifeq ($(shell uname), Darwin) # macOS
		G_PLUS_PLUS := g++ -std=gnu++11 -O3 -Wall -g -I $(INC_FOLDER)
		OS_DEFINE := -DMACOS
	else # linux
		G_PLUS_PLUS := g++ -std=gnu++11 -static -O3 -Wall -g -I $(INC_FOLDER)
		OS_DEFINE := -DLINUX
	endif
	EXEC := exec.final
	DELETE := rm -f $(OBJ_FOLDER)/*.o $(EXEC)
	MAKE_FOLDER := mkdir -p $(OBJ_FOLDER)
endif

.PHONY: all
all : final

# specify files
MAIN := main
MAIN_FILE := $(SRC_FOLDER)/$(MAIN).cpp
MAIN_OBJ := $(OBJ_FOLDER)/$(MAIN).o

AI := MyAI
AI_FILE := $(SRC_FOLDER)/$(AI).cpp
AI_HEADER := $(SRC_FOLDER)/$(AI).h
AI_FLAG := 
AI_OBJ := $(OBJ_FOLDER)/$(AI).o

TABLE := Table
TABLE_FILE := $(SRC_FOLDER)/$(TABLE).cpp
TABLE_HEADER := $(SRC_FOLDER)/$(AI).h
TABLE_OBJ := $(OBJ_FOLDER)/$(TABLE).o

# command
$(TABLE_OBJ) : $(TABLE_FILE) $(TABLE_HEADER)
	$(MAKE_FOLDER)
	$(G_PLUS_PLUS) -c $(TABLE_FILE) $(OS_DEFINE) -o $(TABLE_OBJ)

$(AI_OBJ) : $(AI_FILE) $(AI_HEADER) 
	$(MAKE_FOLDER)
	$(G_PLUS_PLUS) -c $(AI_FILE) $(OS_DEFINE) $(AI_FLAG) -o $(AI_OBJ)

$(EXEC) : $(TABLE_OBJ) $(AI_OBJ) $(MAIN_FILE)
	$(MAKE_FOLDER)
	$(G_PLUS_PLUS) -c $(MAIN_FILE) $(OS_DEFINE) -o $(MAIN_OBJ)
	$(G_PLUS_PLUS) $(TABLE_OBJ) $(AI_OBJ) $(MAIN_OBJ) -o $(EXEC)

# target
final : $(EXEC)

# fast : AI_FLAG = -DFAST
# fast : final

# clean object files and executable file
.PHONY: clean
clean: 
	$(DELETE)