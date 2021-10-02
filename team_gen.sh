#!/bin/bash

function random_n {
    echo -e $(seq -s "\n" 1 $1) | sort -R | head -n 1
}

function create_table {
    echo "create table league ("
    echo "    id   serial      primary key,"
    echo "    name varchar(63) not null," 
    echo "    tds  smallint    not null," 
    echo "    run  real        not null,"
    echo "    mvp  smallint    not null"
    echo ");"
}

function generate_players {
    local player_count=$1

    local first_names=( $(< /etc/dictionaries-common/words grep -e ^[A-Z] | grep -v "'s$" | tr -d "'" | sort -R | head -n $player_count) )
    local last_names_a=( $(< /etc/dictionaries-common/words tr -d "'" | sort -R | head -n $player_count) )
    local last_names_b=( $(< /etc/dictionaries-common/words tr -d "'" | sort -R | head -n $player_count) )

    echo "Words total: $(</etc/dictionaries-common/words wc -l)" 1>&2
    echo "First names: ${#first_names[@]}" 1>&2
    echo "Last names:  ${#last_names_a[@]}" 1>&2

    echo "insert into league (name, tds, run, mvp) values"
    for i in $(seq 0 $((player_count - 1)) )
    do
        last_name_a="${last_names_a[i]}"
        last_name_b="${last_names_b[i]}"
        case $(random_n 17) in
        1 |2 |3 |4 |5 )   last_name="${last_name_a^}"                      ;;
        6 |7 |8 |9 |10)   last_name="${last_name_a^}-${last_name_b^}"      ;;
        11|12|13|14|15)   last_name="${last_name_a^}${last_name_b,}"       ;;
        16)               last_name="the ${last_name_b^}"                  ;;
        17)               last_name="\"${last_name_a^}\" ${last_name_b^}" ;;
        esac

        tds=$(( $(random_n 100) - 1 ))
        mvp=$(( $(random_n 10)  - 1 ))
        run=$(random_n 1000)
        separator=$( (( $i != (player_count - 1) )) && echo "," || echo ";" ) 

        player="('${first_names[i]} $last_name', $tds, $run, $mvp)$separator"
        echo ${player}        
    done
}

generate_players $1



