#!/bin/bash

# Output file
output="src.txt"

# Create or clear the output file
: > "$output"

# Enable recursive globbing (for ** to work)
shopt -s globstar nullglob

# List of file patterns to process
file_patterns=(
  "../CMakeLists.txt"
  "../docker-compose.yml"
  "../Dockerfile"
  "../assets/*"
  "../src/*"
  "../po/**/*"
)

# Iterate over each pattern
for pattern in "${file_patterns[@]}"; do
  # Expand pattern and loop through each resulting file
  for file in $pattern; do
    if [ -f "$file" ]; then
      echo "--- File: $file ---" >> "$output"
      cat "$file" >> "$output"
      echo -e "\n" >> "$output"
    fi
  done
done

echo "Content aggregated into $output"
