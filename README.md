####################################################################
# CS:APP Proxy Lab
#
# Student Source Files
####################################################################

This directory contains the files you will need for the CS:APP Proxy
Lab.

proxy.c
csapp.h
csapp.c
    These are starter files.  csapp.c and csapp.h are described in
    your textbook. 

    You may make any changes you like to these files.  And you may
    create and handin any additional files you like.

    Please use `port-for-user.pl' or 'free-port.sh' to generate
    unique ports for your proxy or tiny server. 

Makefile
    This is the makefile that builds the proxy program.  Type "make"
    to build your solution, or "make clean" followed by "make" for a
    fresh build. 

    Type "make handin" to create the tarfile that you will be handing
    in. You can modify it any way you like. Your instructor will use your
    Makefile to build your proxy from source.

port-for-user.pl
    Generates a random port for a particular user
    usage: ./port-for-user.pl <userID>

free-port.sh
    Handy script that identifies an unused TCP port that you can use
    for your proxy or tiny. 
    usage: ./free-port.sh

driver.sh
    The autograder for Basic, Concurrency, and Cache.        
    usage: ./driver.sh

nop-server.py
     helper for the autograder.         

tiny
    Tiny Web server from the CS:APP text

-----

# 🖥️ CS:APP 프록시 랩 (Proxy Lab)
## 학생용 소스 파일

이 디렉터리에는 CS:APP 프록시 랩 과제를 수행하는 데 필요한 파일들이 포함되어 있습니다.

---

* **`proxy.c` `csapp.h` `csapp.c`**
    * 기본으로 제공되는 **시작 파일**들입니다. `csapp.c`와 `csapp.h`는 교재에 설명되어 있습니다.
    * 이 파일들은 원하는 대로 수정할 수 있으며, 추가적인 파일을 생성하여 제출(handin)해도 됩니다.

* **포트 생성 스크립트 관련 안내**
    * 프록시 또는 tiny 서버를 위한 고유 포트를 생성하려면 `'port-for-user.pl'` 또는 `'free-port.sh'` 스크립트를 사용하십시오.

* **`Makefile`**
    * 프록시 프로그램을 **빌드(build)**하는 makefile입니다.
    * `make`: 솔루션을 빌드합니다.
    * `make clean` 후 `make`: 완전히 새로 빌드(fresh build)합니다.
    * `make handin`: 제출할 **tar 파일**을 생성합니다.
    * 이 파일은 원하는 대로 수정할 수 있습니다. 교수자(instructor)는 여러분이 제출한 Makefile을 사용하여 소스 코드로부터 프록시를 빌드할 것입니다.

* **`port-for-user.pl`**
    * 특정 사용자를 위한 **랜덤 포트**를 생성합니다.
    * 사용법: `./port-for-user.pl`

* **`free-port.sh`**
    * 프록시나 tiny 서버에서 사용할 수 있는 **미사용 TCP 포트**를 찾아주는 유용한 스크립트입니다.
    * 사용법: `./free-port.sh`

* **`driver.sh`**
    * Basic, Concurrency, Cache 항목에 대한 **자동 채점기(autograder)**입니다.
    * 사용법: `./driver.sh`

* **`nop-server.py`**
    * 자동 채점기를 위한 **헬퍼(helper) 프로그램**입니다.

* **`tiny`**
    * CS:APP 교재에 포함된 **Tiny 웹 서버** 실행 파일입니다.