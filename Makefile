### glimit funciton UDF for MySQL
#
# 環境に合わせて以下のパスを修正してください
#
# MYSQL_INC: mysql.h, my_global.hがあるディレクトリ
# MYSQL_BIN: mysql(.exe)のパス
# INSTALL_DIR: UDFをインストールするディレクトリ
#

ifeq ($(findstring Windows, $(OS)), Windows) 

### windows(MinGW)
MYSQL_INC = C:\Program Files\MySQL\MySQL Server 5.0\include
MYSQL_BIN = C:\Program Files\MySQL\MySQL Server 5.0\bin\mysql.exe 
INSTALL_DIR = C:\Program Files\MySQL\MySQL Server 5.0\bin

else

### linux
MYSQL_INC = /usr/include/mysql
MYSQL_BIN = /usr/bin/mysql
INSTALL_DIR = /usr/local/lib/mysql

endif

#
# 以下は必要な場合のみ修正してください
#

CC = gcc
NAME = glimit
CFLAGS = -I. -I"$(MYSQL_INC)" -O2 -Wall

ifeq ($(findstring Windows, $(OS)), Windows) 
SONAME = $(NAME).dll
CFLAGS += -D__STRICT_ANSI__ -DDBUG_OFF
PHPUNIT_OPT = 
else
SONAME = $(NAME).so
CFLAGS += -fPIC
PHPUNIT_OPT = --colors
endif

all: $(SONAME)

$(SONAME): $(NAME).o
ifeq ($(findstring Windows, $(OS)), Windows) 
	dllwrap -k -def $(NAME).def --driver-name $(CC) -o $@ $(NAME).o $(LIBS)
	#dlltool -k -d $(NAME).def -l $(SONAME).a
else
	gcc -shared -o $(SONAME) $(NAME).o $(LIBS)
endif

.c.o:
	$(CC) -c $(CFLAGS) $<

install: $(SONAME)
	cp $(SONAME) "$(INSTALL_DIR)"
	"$(MYSQL_BIN)" -u root -p mysql -e "CREATE FUNCTION $(NAME) RETURNS INTEGER SONAME '$(SONAME)';"

uninstall:
	"$(MYSQL_BIN)" -u root -p mysql -e "DROP FUNCTION $(NAME);"
	if [ -f "$(INSTALL_DIR)/$(SONAME)" ]; then rm "$(INSTALL_DIR)/$(SONAME)"; fi

test:
	phpunit $(PHPUNIT_OPT) ./test.php

clean:
	$(foreach o, $(SONAME) $(SONAME).a $(NAME).o, if [ -f $(o) ]; then rm $(o); fi;)
