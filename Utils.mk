BEG = echo -e -n "  \033[32m$(1)$(2)...\033[0m" ; echo -n > /tmp/.`whoami`-build-errors
END = if [[ -s /tmp/.`whoami`-build-errors ]] ; then \
							echo -e -n "\r\033[1;33m$(1)$(2)\033[0m\n"; \
							cat /tmp/.`whoami`-build-errors; \
			else \
							echo -e -n "\r  \033[1;32m$(1)$(2)\033[0m\033[K\n"; \
			fi

INFO = echo -e -n "\r  \033[1;34m$(1)$(2)\033[0m\n"

ERRORFUNC = echo -e " \033[1;31m! Fatal error encountered.\033[0m"; \
				cat /tmp/.`whoami`-build-errors; \
				exit 1;

ERRORS = 2>>/tmp/.`whoami`-build-errors || { $(call ERRORFUNC) }

ERRORSS = >>/tmp/.`whoami`-build-errors || { $(call ERRORFUNC) }

BEGRM = echo -e -n "  \033[31m$(1)$(2)...\033[0m" && echo -n > /tmp/.`whoami`-build-errors
ENDRM = echo -e -n "\r  \033[1;31m$(1)$(2)\033[0m\033[K\n"
