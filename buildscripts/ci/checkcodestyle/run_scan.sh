SCAN_BIN=$1
UNCRUSTIFY_BIN=$2
CONFIG=$3
UNTIDY_FILE=$4
DIR=$5

echo "SCAN_BIN: $SCAN_BIN"
echo "UNCRUSTIFY_BIN: $UNCRUSTIFY_BIN"
echo "CONFIG: $CONFIG"
echo "UNTIDY_FILE: $UNTIDY_FILE"
echo "DIR: $DIR"

$SCAN_BIN -d $DIR -i $UNTIDY_FILE -e cpp,c,cc,hpp,h | xargs -n 1 -P 16 $UNCRUSTIFY_BIN -c $CONFIG --no-backup -l CPP

code=$?
if [[ ${code} -ne 0 ]]; then
    exit $code
fi

$SCAN_BIN -d $DIR -i $UNTIDY_FILE -e mm | xargs -n 1 -P 16 $UNCRUSTIFY_BIN -c $CONFIG --no-backup -l OC+

code=$?
if [[ ${code} -ne 0 ]]; then
    exit $code
fi
