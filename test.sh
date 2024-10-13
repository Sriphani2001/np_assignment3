# Write your bash script here.
echo "Testing that the client actually prints what the server sends."
echo "This is a dedicated, fake, server."
echo "It "
echo "1. accepts a client,"
echo "2. sends 'HELLO 1\n"
echo "3. validates the NICK name,"
echo "4. sends 'OK\\n',\n"
echo "5. It then sends a partially pre-determined string; "
echo "   'MSG FAKECLIENT Alice Bob Charlie <yyyy-mm-dd> <hh:mm:ss> +RANDOMSTRING' "
echo "6. Once transmitted, it waits for a reply from the client, but it will mostlikely "
echo "   timeout after 3s, at that time it closes the client socket and exits. "
echo " "
echo "We will be looking for that string in the client output. "

echo "Starting server, and logging it to file."
randomString=$(tr -dc A-Za-z0-9 </dev/urandom | head -c 13)

./kiss_server.pl 127.0.0.1 5400 "$randomString" &>kiss_server.log &

KSERVER_PID=$!
echo "Sleep abit."
sleep 1
echo "PID = $KSERVER_PID "
echo "Checking kiss_server.log"
   

SRV_PID=$(lsof -i:5400 | grep kiss_server | awk '{print $2}')
echo "Check that server started, SRV_PID = $KSERVER_PID ".


if [ -z "$KSERVER_PID" ]; then
    echo "There does not seem to be a server running on $serverport". 
    echo "Server Evaluation: Serverfailed, did not start " 
    lsof -i:$serverport 
    echo "kiss_server.log=> " $(cat kiss_server.log) " EOL"
    echo "Logging server as failed. " 
    echo " "
    exit 1
fi

   
echo "Checking server.log"
##Is serverlog open, if so server is running.
servStat=$(lsof -- kiss_server.log) 
if [ -z "$servStat" ]; then
    echo "The server.log file was closed. " 
    echo "kiss_server.log=> " $(cat kiss_server.log) " EOL"
    echo "Server Evaluation: Serverfailed, did not start " 
    lsof -i:5400 
    echo "Logging server as failed. " 
    exit 1
fi

echo "starting cchat"
timeout 15 unbuffer ./cchat 127.0.0.1:5400 Dave | tee output.txt
returnVal=$?

echo "Return Val (should be != 0 ) $returnVal"
echo "Got this:<begin>"
cat output.txt
echo "</end>"

match=$(grep -i FAKECLIENT output.txt)
if [ ! -z "$match" ]; then
    echo "Got a message from FAKECLIENT, as expected."
    echo "I'm also checking for a random string '$randomString'. "
    match2=$(grep -i "$randomString" output.txt)
    if [ ! -z "$match2" ]; then
        echo "Found it!"
    else 
        echo "Missing the random string."
        echo "Problems, did not get the expected message."
        cat output.txt
        echo "<end>"
        echo "I'm looking for a randomString( $randomString) mentioned above"
        exit 1;
    fi        
else
    echo "Problems, did not get the expected message."
    cat output.txt
    echo "<end>"
    echo "I'm looking for the string mentioned above"
    echo "You need to fix that. "
    exit 1;
fi