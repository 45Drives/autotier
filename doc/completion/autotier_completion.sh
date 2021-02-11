_AutotierCompleteBash () {
	local cur
	local prev
	local file_arg_commands
	local before_prev
	
	COMPREPLY=()
	
	cur=${COMP_WORDS[COMP_CWORD]}
	prev=${COMP_WORDS[COMP_CWORD-1]}
	before_prev=${COMP_WORDS[COMP_CWORD-2]}
	
	file_arg_commands=("-c" "--config" "unpin" "which-tier")
	if [[ " ${file_arg_commands[@]} " =~ " ${prev} "  || "$before_prev" == "pin" ]]; then
		COMPREPLY=()
	else
		case "$cur" in
			-*)
			COMPREPLY=(
				$(compgen -W '-c --config -h --help -o --fuse-options -q --quiet -v --verbose -V --version' -- $cur)
			)
			;;
			*)
			COMPREPLY=(
				$(compgen -W 'config help list-pins list-popularity oneshot pin status unpin which-tier' -- $cur)
			)
			;;
		esac
	fi
}

_AutotierCompleteZsh () {
	local cur
	local words
	local prev
	local file_arg_commands
	local before_prev
	
	read -cA words
	read -cn pos
	
	reply=()
	
	cur=$1
	
	prev=${words[$pos-2]}
	before_prev=${words[$pos-3]}
# 	echo prev = $prev
# 	echo cur = $cur
	
	file_arg_commands=("-c" "--config" "unpin" "which-tier")
	if [[ " ${file_arg_commands[@]} " =~ " ${prev} "  || "$before_prev" == "pin" ]]; then
		reply=()
	else
		case "$cur" in
			-*)
			reply=(
				$(compgen -W '-c --config -h --help -o --fuse-options -q --quiet -v --verbose -V --version' -- $cur)
			)
			;;
			*)
			reply=(
				$(compgen -W 'config help list-pins list-popularity oneshot pin status unpin which-tier' -- $cur)
			)
			;;
		esac
	fi
}

if [[ "$SHELL" == "$(which bash)" || "$SHELL" == "/bin/bash" ]]; then
	complete -F _AutotierCompleteBash -o default autotier
elif [[ "$SHELL" == "$(which zsh)" ]]; then
	compctl -K _AutotierCompleteZsh + -f autotier
fi

