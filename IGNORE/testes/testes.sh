#!/bin/bash

# Add missing import
# command -v ../../ems >/dev/null 2>&1 || { echo >&2 "ems command not found. Aborting."; exit 1; }

../../ems ./ 999999999 999999999

for i in {1..25}; do
  if [[ ($i -ge 1 && $i -le 7) || $i -eq 16 || $i -eq 21 || $i -eq 23 || $i -eq 25 ]]; then
    # Use the diff command to compare the files
    resultado=$(diff "$i.out" "$i.result")

    # Check for differences
    if [ -n "$resultado" ]; then
      # If there are differences, save the result to a file
      echo "$resultado" > "$i.diff"
      # Print a special message
      echo "Teste $i falhou."
    else
      # If there are no differences do nothing
      echo "Teste $i passou."
    fi
  fi
done
