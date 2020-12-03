#!/bin/sh
## iostat csv generator ##
# This script converts iostat output every second in one line and generates a csv file.

while getopts "p:" opt; do
    case $opt in
        p) devices="$devices"" -p ""$OPTARG";;
        #...
    esac
done
shift $((OPTIND -1))

# Settings
COMMAND_OUTPUT=`iostat -t -x $devices | grep -v -e "^\s*$"`
TITLE_LINE_HEADER="Date Time"
DEVICE_LINE_OFFSET=`echo "$COMMAND_OUTPUT" | grep -n Device | cut -f1 -d:`
# Count command output line number
COMMAND_OUTPUT_LINE_NUMBER=`wc -l <<EOF
${COMMAND_OUTPUT}
EOF`
RAW_DEVICES_LINE_NUMBER=`expr ${COMMAND_OUTPUT_LINE_NUMBER} - ${DEVICE_LINE_OFFSET}`
DEVICE_LINE_NUMBER=`expr ${RAW_DEVICES_LINE_NUMBER} + 1`

# Calcurate how many lines to concatenate in each second
CONCAT_LINE_NUMBER=`expr ${RAW_DEVICES_LINE_NUMBER} + 2`

# Print "N;" for ${CONCAT_LINE_NUMBER} times to concatenate output lines by using sed command
CONCAT_LINE_N=`seq -s'N;' ${CONCAT_LINE_NUMBER} | tr -d '[:digit:]'`

# Generate title line for csv
TITLE_AVG_CPU=`grep avg-cpu <<EOF
${COMMAND_OUTPUT}
EOF`
TITLE_EACH_DEVICE=`grep "Device" <<EOF
${COMMAND_OUTPUT}
EOF`
DEVICE_LINES=`echo "$COMMAND_OUTPUT" | tail -n "$RAW_DEVICES_LINE_NUMBER"`
DEVICES_NAMES=($(echo "$DEVICE_LINES" | awk 'BEGIN {RS="\n";FS="[\\ \t]+"} {print $1}'))
NUMBER_DEVICE_FIELDS=`echo "$DEVICE_LINES" | head -n 1 | awk 'BEGIN {RS="\n";FS="[\\ \t]+"} {print NF}'`
NUMBER_TIME_FIELDS=`echo ${TITLE_LINE_HEADER} | awk 'BEGIN {RS="\n";FS="[\\ \t]+"} {print NF}'`
NUMBER_CPU_FIELDS=`echo ${TITLE_AVG_CPU} | sed 's/^[^%]*//1' | awk 'BEGIN {RS="\n";FS="[\\ \t]+"} {print NF}'`
TITLE=""
TITLE=$TITLE"System"`seq -s"," $NUMBER_TIME_FIELDS | tr -d [:digit:]`","
TITLE=$TITLE"Avg Cpu"`seq -s"," $NUMBER_CPU_FIELDS | tr -d [:digit:]`","
for (( i=0; i<${#DEVICES_NAMES[@]}; i++ )); do TITLE=$TITLE"${DEVICES_NAMES[$i]}"`seq -s"," $NUMBER_DEVICE_FIELDS | tr -d [:digit:]`","; done
TITLE=`echo $TITLE | sed 's/,$//1'` # remove last ","
TITLE_DEVICES=`seq -s"${TITLE_EACH_DEVICE} " ${DEVICE_LINE_NUMBER} | tr -d '[:digit:]'`

echo "${TITLE}"
echo "${TITLE_LINE_HEADER} ${TITLE_AVG_CPU} ${TITLE_DEVICES}" \
 | awk 'BEGIN {OFS=","} {$1=$1;print $0}' | sed 's/avg-cpu//g;s/://g;s/,,/,/g'

MAIN_COMMAND="iostat -t -x 1 $devices"
# Main part
LANG=C; eval "$MAIN_COMMAND" | grep --line-buffered -v -e avg-cpu -e Device -e Linux -e "^\s*$" \
| sed --unbuffered "${CONCAT_LINE_N}s/,/\./g;s/\n/,/g;s/\s\s*/,/g;s/,,*/,/g;s/^,//g;s/,$//g"
# grep -v -e avg-cpu -e Device -e Linux
#  => Exclude title columns

# sed --unbuffered
#  '${CONCAT_LINE_N}s/\n/,/g'
#    => Read ${CONCAT_LINE_NUMBER} lines and replace newline characters to ","

#  's/\s\s*/,/g'
#    => Replace adjacent blank symbols for ","

#  's/,,*/,/g'
#    => Replace adjacent commas for single comma

#  's/^,//g'
#    => Remove comma symbol placed at the beginning of the line
