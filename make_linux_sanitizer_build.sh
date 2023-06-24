if [ "$#" = "0" ]; then
	exit $result
else
	make sanitizer.$1.sanitizer
fi
