# #!/bin/bash

# pwd
# cd ..
# cp ./src/main.cpp ./test/

#!/bin/bash

cd ..
# Specify the folder path
folder_path="test"

# Populate the array with filenames from the folder
files=()
while IFS= read -r -d '' file; do
  files+=("$file")
done < <(find "$folder_path" -maxdepth 1 -type f -print0)

# Print the options
for ((i=0; i<${#files[@]}; i++)); do
  echo "$i. ${files[$i]}"
done

# Prompt for user input
read -p "Enter the option number: " option

# Validate the input
if [[ $option =~ ^[0-9]+$ && $option -ge 0 && $option -lt ${#files[@]} ]]; then
  # Open the selected file
#   echo "Opening ${files[$option]}"
  cp ./src/main.cpp ./test/
  cp ${files[$option]} ./src/main.cpp
  # Add the command to open the file here (e.g., open "${files[$option]}" on macOS)
else
  echo "Invalid option."
fi