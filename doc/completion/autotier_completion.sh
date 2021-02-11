_AutotierCompleteBash () {
	local cur
	
	COMPREPLY=()
	
	cur=${COMP_WORDS[COMP_CWORD]}
	
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
	esac
}

_AutotierCompleteZsh () {
	local cur
	
	reply=()
	
	cur=$1
	
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
	esac
}

if [[ "$SHELL" == "$(which bash)" || "$SHELL" == "/bin/bash" ]]; then
	complete -F _AutotierCompleteBash autotier
elif [[ "$SHELL" == "$(which zsh)" ]]; then
	compctl -K _AutotierCompleteZsh autotier
fi

