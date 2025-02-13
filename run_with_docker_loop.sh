#!/bin/bash

docker compose up -d --build

if [[ $? != 0 ]]
then
	echo "Nu s-a putut crea/porni containerul Docker"
	docker compose down
	cd ..
	exit
fi


# Run the inner script 100 times
# Run the inner script 100 times
smaller_total_count=0
for i in {1..50}
do
    # echo "Running iteration $i..."
    # Capture the full output
    
    full_output=$(docker exec -w /apd/checker -it apd_container /apd/checker/checker.sh)

    if [[ $? != 0 ]]; then
        echo "Iteration $i failed. Exiting loop."
        break
    fi
    
    # Extract the relevant lines
    output=$(echo "$full_output" | grep -E "Total:")

    # Extract the "Total:" value and check if it's different from 84/84
    total=$(echo "$output" | grep "Total:" | awk '{print $NF}' | tr -d '[:space:]')

    if [[ "$total" != "40/40" ]]; then
        echo "Iteration $i - Total is different than expected!"
        echo "Full output:"
        echo "$full_output"
        ((smaller_total_count++))
    # else
    #     echo "$output"
    fi
done
echo "Number of results with smaller total: $smaller_total_count"
# docker exec -w /apd/checker -it apd_container /apd/checker/checker.sh
docker compose down
cd ..