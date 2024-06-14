const fs = require('fs');
const { jsonrepair } = require('jsonrepair');

// This script processes command-line arguments
const filename = process.argv[2];  // Skip the first two elements
// Simple processing: join arguments into a stri

function processFile(filePath) {
    try {
        const data = fs.readFileSync(filePath, { encoding: 'utf8' });
        const repairData = jsonrepair(data);
        console.log(repairData);
        fs.unlinkSync(filePath);
        return repairData;
    } catch (error) {
        console.error('Error reading file:', error);
        return '';
    }
}


processFile(filename);
// Output the result
