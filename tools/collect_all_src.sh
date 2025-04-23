# Create or overwrite the output file
> src.txt

# Loop through all the files and patterns provided
for file in ../CMakeLists.txt ../docker-compose.yml ../Dockerfile ../assets/* ../src/*; do
  # Check if it's actually a file (and not a directory or non-existent)
  if [ -f "$file" ]; then
    echo "--- File: $file ---" >> src.txt # Print the header with the relative path
    cat "$file" >> src.txt             # Append the file content
    echo "" >> src.txt                 # Add a blank line for separation (optional)
  fi
done

echo "Content aggregated into src.txt"