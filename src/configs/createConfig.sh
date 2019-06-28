#!/bin/bash
#################################
# Contacts: Eunchong Kim, Arisa Kubota
# Email: eunchong.kim at cern.ch, arisa.kubota at cern.ch
# Date: April 2019
# Project: Local Database for Yarr
# Description: Create config file 
# Usage: ./createConfig.sh [-a <rd53a/fei4b>*] [-m <SerialNumber>*] [-c <ChipNum>] [-r <ControllerCfg>] [-d] [-R]
################################

# Change fixed tx channel and rx start channel
tx_fix=0
rx_start=0

# default parameters
reset=false
now=`date +"%y%m%d%H%M"`

# Usage
function usage {
    cat <<EOF

Usage:
    ./$(basename ${0}) [-a <rd53a/fei4b>*] [-m SN*] [-c Chips] [-r Controller] [-d] [-R]

Options:
    -a <rd53a/fei4b>  asic type (*req.)
    -m <str>          serial number (*req.)
    -c <int>          number of chips (req. in first creation)
    -r <path>         controller config file   default: ./controller/specCfg.json
    -d <user account> upload into databse
    -R                reset all config files

EOF
}

while getopts a:m:c:r:d:RI: OPT
do
    case ${OPT} in
        a ) asic=${OPTARG} ;;
        m ) sn=${OPTARG} ;;
        c ) chips=${OPTARG} ;;
        r ) controller=${OPTARG} ;;
        d ) account=${OPTARG} ;;
        R ) reset=true ;;
        * ) usage
            exit ;;
    esac
done

# asic type
if [ -z ${asic} ]; then
    echo "Please give \"rd53a\" or \"fei4b\" with '-a'."
    usage
    exit
elif [ ${asic} == "rd53a" ]; then
    chiptype="RD53A"
    chipid="ChipId"
    name="Name"
    john="JohnDoe"
elif [ ${asic} == "fei4b" ]; then
    chiptype="FEI4B"
    chipid="chipId"
    name="name"
    john="JohnDoe"
else
    echo "Please give \"rd53a\" or \"fei4b\" with '-a'."
    usage
    exit
fi

# serial number
if [ -z ${sn} ]; then
    echo "Please give serial number with '-m'."
    usage
    exit
fi

# reset directory
if "${reset}"; then
    echo "Reset config files of ${sn}."
    if [ -d ${asic}/${sn} ]; then
        echo "Move config files to backup directory before reset."
        mkdir -p ${asic}/${sn}/backup/${now}
        rsync -a ${asic}/${sn}/ ${asic}/${sn}/backup/${now} --exclude '/backup/'
        if [ -f ${asic}/${sn}/connectivity.json ]; then
            rm ${asic}/${sn}/connectivity.json
            rm ${asic}/${sn}/${sn}_chip*.json
        else
            echo "Not exist connectivity.json and nothing was done."
        fi
    else
        echo "Not exist config files of ${sn} and nothing was done."
    fi
    echo "Continue to remake config files ... [y/n] "
    read answer
    while [ -z ${answer} ]; 
    do
        echo "Continue to remake config files ... [y/n] "
        read answer
    done
    if [ "${answer}" == "n" ]; then
        exit
    elif [ "${answer}" != "y" ]; then
        echo "Unexpected token ${answer}"
        exit
    fi
fi

# Make directory for config file
if [ -f ${asic}/${sn}/connectivity.json ]; then
    echo "Config files of ${sn} are already exist, then move it to backup directory"
    mkdir -p ${asic}/${sn}/backup/${now}
    rsync -a ${asic}/${sn}/ ${asic}/${sn}/backup/${now} --exclude '/backup/'

    # Make controller for this module
    if [ ! -z ${controller} ]; then
        if [ ${controller} != ${asic}/${sn}/controller.json ]; then
            echo "Created ${asic}/${sn}/controller.json"
            cp ${controller} ${asic}/${sn}/controller.json
        fi
    fi

    # Make connectivity config
    for file in `\find ${asic}/${sn} -path "${asic}/${sn}/${sn}_chip*.json"` ; do
        cfgname=`cat ${file} | grep ${name} | awk -F'["]' '{print $4}'`
        cfgid=`cat ${file} | grep \"${chipid}\" | awk '{print $2}'`
        echo "Reset ${file##*/}"
        cp defaults/default_${asic}.json ${file}
        sed -i "/${chipid}/s/0,/${cfgid}/g" ${file}
        sed -i "/${name}/s/JohnDoe/${cfgname}/g" ${file}
    done
else    # first creation
    # chips
    if [ -z ${chips} ]; then
        echo "Please give the number of chips with '-c'."
        usage
        exit
    fi
    echo ${chips} | grep [^0-9] > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "Please give an integral as number of chips with '-c'. "
        usage
        exit
    fi

    # controller config
    if [ -z ${controller} ]; then
        controller=controller/specCfg.json
    fi
    if [ ! -f ${controller} ]; then
        echo "Not exist controller \"${controller}\"."
        usage
        exit
    fi

    cnt=0
    chipIds=()
    chipNames=()
    while [ ${cnt} -lt ${chips} ]; do
        cnt=$(( cnt + 1 ))
        chipName="${sn}_chip${cnt}"
        echo "Create ${asic}/${sn}/${chipName}.json"
        echo "---------------------"
        echo "chip serial number: ${chipName}"
        echo "---------------------"
        chipNames+=( ${chipName} )
        echo "Set chipId for chip${cnt}... [#(chipId)] "
        read answer
        unset id
        while [ -z ${id} ]; 
        do
            if [ -z ${answer} ]; then 
                echo "Set chipId ... [#(chipId)] "
                read answer
            else
                echo ${answer} | grep [^0-9] > /dev/null 2>&1
                if [ $? -eq 0 ]; then
                    echo "Please give an integral as chipId. "
                    unset answer
                    read answer
                else
                    id=${answer}
                fi
            fi
        done
        chipIds+=( ${id} )
    done

    # Confirmation
    cnt=0
    echo " "
    echo "serial number: ${sn}"
    echo "controller config: ${controller}"
    echo "num of chips: ${chips}"
    while [ ${cnt} -lt ${chips} ]; do
        cnt=$(( cnt + 1 ))
        echo "** ${sn}_chip${cnt}"
        echo "    chip serial number: ${chipNames[$((cnt-1))]}"
        echo "    chipId: ${chipIds[$((cnt-1))]}"
    done
    echo " "
    echo "Make sure? [y/n]"
    read answer
    while [ -z ${answer} ]; 
    do
        echo "Make sure? [y/n] "
        read answer
    done
    if [ "${answer}" == "n" ]; then
        exit
    elif [ "${answer}" != "y" ]; then
        echo "Unexpected token ${answer}"
        exit
    fi

    # Make config directory
    if [ ! -d ${asic}/${sn} ]; then
        echo "Created config directory ${asic}/${sn}"
        mkdir -p ${asic}/${sn}/backup
    fi

    # Make controller for this module
    if [ ${controller} != ${asic}/${sn}/controller.json ]; then
        echo "Created ${asic}/${sn}/controller.json"
        cp ${controller} ${asic}/${sn}/controller.json
    fi
    
    # Make connectivity.json
    echo "Created ${asic}/${sn}/connectivity.json"
    echo "{" > ${asic}/${sn}/connectivity.json
    echo "    \"stage\": \"Testing\"," >> ${asic}/${sn}/connectivity.json
    echo "    \"module\": {" >> ${asic}/${sn}/connectivity.json
    echo "        \"serialNumber\": \"${sn}\"," >> ${asic}/${sn}/connectivity.json
    echo "        \"componentType\": \"Module\"" >> ${asic}/${sn}/connectivity.json
    echo "    }," >> ${asic}/${sn}/connectivity.json
    echo "    \"chipType\" : \"${chiptype}\"," >> ${asic}/${sn}/connectivity.json

    cnt=0
    echo "    \"chips\" : [" >> ${asic}/${sn}/connectivity.json
    while [ ${cnt} -lt ${chips} ]; do
        cnt=$(( cnt + 1 ))
        echo "Created ${sn}_chip${cnt}.json"
        cp defaults/default_${asic}.json ${asic}/${sn}/${sn}_chip${cnt}.json
        sed -i "/${chipid}/s/0/${chipIds[$((cnt-1))]}/g" ${asic}/${sn}/${sn}_chip${cnt}.json
        sed -i "/${name}/s/JohnDoe/${chipNames[$((cnt-1))]}/g" ${asic}/${sn}/${sn}_chip${cnt}.json
    
        rx_ch=$(( cnt - 1 + rx_start ))
        echo "        {" >> ${asic}/${sn}/connectivity.json
        echo "            \"serialNumber\": \"${chipNames[$((cnt-1))]}\"," >> ${asic}/${sn}/connectivity.json
        echo "            \"componentType\": \"Front-end Chip\"," >> ${asic}/${sn}/connectivity.json
        echo "            \"chipId\": ${chipIds[$((cnt-1))]}," >> ${asic}/${sn}/connectivity.json
        echo "            \"config\" : \"configs/${asic}/${sn}/${sn}_chip${cnt}.json\"," >> ${asic}/${sn}/connectivity.json
        echo "            \"tx\" : ${tx_fix}," >> ${asic}/${sn}/connectivity.json
        echo "            \"rx\" : ${rx_ch}" >> ${asic}/${sn}/connectivity.json
        if [ ${cnt} -ne ${chips} ]; then
            echo "        }," >> ${asic}/${sn}/connectivity.json
        else
            echo "        }" >> ${asic}/${sn}/connectivity.json
        fi
    done
    echo "    ]" >> ${asic}/${sn}/connectivity.json
    echo "}" >> ${asic}/${sn}/connectivity.json
fi

cd ../

# Register module and chips component to DB
if [ ! -z ${account} ]; then
    echo "./bin/dbAccessor -C configs/${asic}/${sn}/connectivity.json -u ${account}"
    ./bin/dbAccessor -C configs/${asic}/${sn}/connectivity.json -u ${account}
fi