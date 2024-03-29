//nc2csv.c
//Copyright 2012, Allen Jordan, allen.jordan@gmail.com

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#include <sys/types.h>
//#include <dirent.h>
#include <netcdf.h>

//string buffer for print formating
//#define STR_LENGTH	100
//char str[STR_LENGTH];

//todo: make the program exit somehow
void HandleNCError(char* funcName, int status)
{
	printf("NetCDF error in: %s with status: %d\n", funcName, status);
	exit(status);
}

//storage for individual NetCDF variable raw data
typedef struct
{
	nc_type type;
	void *data;
} VariableData;

int main (int argc, char** argv)
{
	//define some generic loop indices
	int i, j;
	
	//make sure a filename was provided
	if (argc < 2)
	{
		puts("NetCDF filename argument required");
		return -1;
	}
	
	//loop through every input file
	int argIndex;
	for (argIndex = 1; argIndex < argc; argIndex++)
	{
		char* filename = argv[argIndex];
		size_t filenameLength = strlen(filename);
		
		//allocate space for the CSV filename, plus some room for the longer extension, etc
		char *csvFilename = malloc((filenameLength + 5)*sizeof(char));
		strcpy(csvFilename, filename);
		char *periodLocation = strrchr(csvFilename, '.');
		*periodLocation = '\0';
		strcat(csvFilename, ".csv");
		
		//open the NetCDF file/dataset
		int datasetID;
		int ncResult;
		ncResult = nc_open(filename, NC_NOWRITE, &datasetID);
		if (ncResult != NC_NOERR) HandleNCError("nc_open", ncResult);
		
		printf("opened NetCDF file: %s", filename);
		printf("output CSV filename: %s\n", csvFilename);
		
		//get basic information about the NetCDF file
		int numDims, numVars, numGlobalAtts, unlimitedDimID;
		ncResult = nc_inq(datasetID, &numDims, &numVars, &numGlobalAtts, &unlimitedDimID);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq", ncResult);
		int formatVersion;
		ncResult = nc_inq_format(datasetID, &formatVersion);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_format", ncResult);
		
		if (numDims != 1)
		{
			puts("error: only 1-dimensional NetCDF files are supported for now");
			return -1;
		}
		
		//show some of the NetCDF file information on the console
		printf("# dims: %d\n# vars: %d\n# global atts: %d\n", numDims, numVars, numGlobalAtts);
		if (unlimitedDimID != -1) puts("contains unlimited dimension");
		switch (formatVersion)
		{
			case NC_FORMAT_CLASSIC:
				puts("classic file format");
				break;
			case NC_FORMAT_64BIT:
				puts("64-bit file format");
				break;
			case NC_FORMAT_NETCDF4:
				puts("netcdf4 file format");
				break;
			case NC_FORMAT_NETCDF4_CLASSIC:
				puts("netcdf4 classic format");
				break;
			default:
				puts("unrecognized file format");
				return -1;
				break;
		}
		
		//int dimID;
		//for (dimID = 0; dimID < numDims; dimID++)
		//{
		//get dimension names and lengths
		char dimName[NC_MAX_NAME+1];
		size_t dimLength;
		ncResult = nc_inq_dim(datasetID, 0, dimName, &dimLength);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_dim", ncResult);
		
		printf("dimension: %s length: %d\n", dimName, dimLength);
		//}
		
		//open/create the CSV file for outputting data
		//todo: better file name
		FILE *csvFile = fopen(csvFilename, "w");
		
		//output the global attributes
		//todo: output more than just the text-based ones
		for (i=0; i<numGlobalAtts; i++)
		{
			char attName[NC_MAX_NAME+1];
			ncResult = nc_inq_attname(datasetID, NC_GLOBAL, i, attName);
			if (ncResult != NC_NOERR) HandleNCError("nc_inq_attname", ncResult);
			size_t attLength;
			ncResult = nc_inq_attlen(datasetID, NC_GLOBAL, attName, &attLength);
			if (ncResult != NC_NOERR) HandleNCError("nc_inq_attlen", ncResult);
			
			char *attValue = malloc(attLength*sizeof(char));
			
			//only output the attribute if it can be converted into text
			ncResult = nc_get_att_text(datasetID, NC_GLOBAL, attName, attValue);
			if (ncResult == NC_NOERR)
			{
				fprintf(csvFile, "%s, %s\r\n", attName, attValue);
			}
			
			free(attValue);
		}
		fprintf(csvFile, "\r\n");
		
		//storage for all variables in the NetCDF file
		//todo: watch out for segfaults, maybe use nc_get_vara_ to get pieces instead of whole variables
		VariableData **variableDataList = (VariableData **)malloc(numVars * sizeof(VariableData*));
		char **standardNameList = (char **)malloc(numVars * sizeof(char*));
		char **longNameList = (char **)malloc(numVars * sizeof(char*));
		char **unitStringList = (char **)malloc(numVars * sizeof(char*));
		
		//loop through all the variables
		int varID;
		for (varID=0; varID<numVars; varID++)
		{
			//get information about the variable
			char varName[NC_MAX_NAME+1];
			nc_type varType;
			int numVarDims;
			int varDimIDs[NC_MAX_VAR_DIMS];
			int numVarAtts;
			ncResult = nc_inq_var(datasetID, varID, varName, &varType, &numVarDims, varDimIDs, &numVarAtts);
			if (ncResult != NC_NOERR) HandleNCError("nc_inq_var", ncResult);
			
			//output variable info to console
			printf("variable: %s # dims: %d # atts: %d type: %d\n", varName, numVarDims, numVarAtts, (int)varType);
			
			//output variable name to the CSV file
			fprintf(csvFile, "%s", varName);
			if (varID != (numVars-1)) fprintf(csvFile, ", ");
			
			//see if there is an attribute for the variable standard name, and store it if there is
			int standardNameAttLen = 0;
			ncResult = nc_inq_attlen(datasetID, varID, "standard_name", &standardNameAttLen);
			if (ncResult == NC_NOERR)
			{
				//allocate enough space for the units string
				char *standardNameValueStr = (char *) malloc(standardNameAttLen + 1);
				ncResult = nc_get_att_text(datasetID, varID, "standard_name", standardNameValueStr);
				if (ncResult != NC_NOERR) HandleNCError("nc_get_att_text", ncResult);
				//make sure the string is null terminated (sometimes it won't be, apparently)
				standardNameValueStr[standardNameAttLen] = '\0';
				standardNameList[varID] = standardNameValueStr;
			}
			else
			{
				char *emptyHeapStr = malloc(1*sizeof(char));
				emptyHeapStr[0] = '\0';
				standardNameList[varID] = emptyHeapStr;
			}
			
			//see if there is an attribute for the variable long name, and store it if there is
			int longNameAttLen = 0;
			ncResult = nc_inq_attlen(datasetID, varID, "long_name", &longNameAttLen);
			if (ncResult == NC_NOERR)
			{
				//allocate enough space for the units string
				char *longNameValueStr = (char *) malloc(longNameAttLen + 1);
				ncResult = nc_get_att_text(datasetID, varID, "long_name", longNameValueStr);
				if (ncResult != NC_NOERR) HandleNCError("nc_get_att_text", ncResult);
				//make sure the string is null terminated (sometimes it won't be, apparently)
				longNameValueStr[longNameAttLen] = '\0';
				longNameList[varID] = longNameValueStr;
			}
			else
			{
				char *emptyHeapStr = malloc(1*sizeof(char));
				emptyHeapStr[0] = '\0';
				longNameList[varID] = emptyHeapStr;
			}
			
			//see if there is an attribute for the variable units description, and store it if there is
			int unitsAttLen = 0;
			ncResult = nc_inq_attlen(datasetID, varID, "units", &unitsAttLen);
			if (ncResult == NC_NOERR)
			{
				//allocate enough space for the units string
				char *unitsValueStr = (char *) malloc(unitsAttLen + 1);
				ncResult = nc_get_att_text(datasetID, varID, "units", unitsValueStr);
				if (ncResult != NC_NOERR) HandleNCError("nc_get_att_text", ncResult);
				//make sure the string is null terminated (sometimes it won't be, apparently)
				unitsValueStr[unitsAttLen] = '\0';
				unitStringList[varID] = unitsValueStr;
			}
			else
			{
				char *emptyHeapStr = malloc(1*sizeof(char));
				emptyHeapStr[0] = '\0';
				unitStringList[varID] = emptyHeapStr;
			}
			
			
			//make sure the variable only has 1 dimension
			if (numVarDims != 1) puts("warning: only 1-dimensional variables are supported for now... skipping");
			else
			{
				//storage for this variable's data structure
				VariableData *variableData = (VariableData *)malloc(sizeof(VariableData));
				variableData->type = varType;
				
				//depending on the type, allocate storage for this variable's raw data contained in the structure
				switch (varType)
				{
					case NC_BYTE:
					{
						variableData->data = malloc(dimLength * sizeof(unsigned char));
						ncResult = nc_get_var_uchar(datasetID, varID, (unsigned char*)variableData->data);
						if (ncResult != NC_NOERR) HandleNCError("nc_get_var", ncResult);
						break;
					}
					case NC_CHAR:
					{
						variableData->data = malloc(dimLength * sizeof(char));
						ncResult = nc_get_var_text(datasetID, varID, (char*)variableData->data);
						if (ncResult != NC_NOERR) HandleNCError("nc_get_var", ncResult);
						break;
					}
					case NC_SHORT:
					{
						variableData->data = malloc(dimLength * sizeof(short));
						ncResult = nc_get_var_short(datasetID, varID, (short*)variableData->data);
						if (ncResult != NC_NOERR) HandleNCError("nc_get_var", ncResult);
						break;
					}
					case NC_INT:
					{
						variableData->data = malloc(dimLength * sizeof(int));
						ncResult = nc_get_var_int(datasetID, varID, (int*)variableData->data);
						if (ncResult != NC_NOERR) HandleNCError("nc_get_var", ncResult);
						break;
					}
					case NC_FLOAT:
					{
						variableData->data = malloc(dimLength * sizeof(float));
						ncResult = nc_get_var_float(datasetID, varID, (float*)variableData->data);
						if (ncResult != NC_NOERR) HandleNCError("nc_get_var", ncResult);
						break;
					}
					case NC_DOUBLE:
					{
						variableData->data = malloc(dimLength * sizeof(double));
						ncResult = nc_get_var_double(datasetID, varID, (double*)variableData->data);
						if (ncResult != NC_NOERR) HandleNCError("nc_get_var", ncResult);
						break;
					}
					default:
					{
						puts("warning: invalid variable type");
						break;
					}
				}//end of type switch
				
				//store the variable data structure in the list of all variable data structures, to be used later when outputting
				variableDataList[varID] = variableData;
				
			}//end of num var dimensions check
		}//end of variable loop
		
		//output a newline after the variable names CSV header line
		fprintf(csvFile, "\r\n");
		
		//output the variable standard names
		for (i=0; i<numVars; i++)
		{
			fprintf(csvFile, "%s", standardNameList[i]);
			if (i != (numVars-1)) fprintf(csvFile, ", ");
		}
		fprintf(csvFile, "\r\n");
		
		//output the variable long names
		for (i=0; i<numVars; i++)
		{
			fprintf(csvFile, "%s", longNameList[i]);
			if (i != (numVars-1)) fprintf(csvFile, ", ");
		}
		fprintf(csvFile, "\r\n");
		
		//output the variable units
		for (i=0; i<numVars; i++)
		{
			char *unitsName = unitStringList[i];
			if (unitsName[0] != '[')
				fprintf(csvFile, "[");
			
			fprintf(csvFile, "%s", unitsName);
			
			if (unitsName[strlen(unitsName)-1] != ']')
				fprintf(csvFile, "]");
			
			if (i != (numVars-1)) fprintf(csvFile, ", ");
		}
		fprintf(csvFile, "\r\n");
		
		
		//output variable data to the CSV file
		for (i=0; i<dimLength; i++)
		{
			for (j=0; j<numVars; j++)
			{
				VariableData *variableData = variableDataList[j];
				//write data in the correct format for the variable's type
				switch (variableData->type)
				{
					case NC_BYTE:
					{
						unsigned char *byteList = (unsigned char *)variableData->data;
						fprintf(csvFile, "%u", byteList[i]);
						break;
					}
					case NC_CHAR:
					{
						char *byteList = (char *)variableData->data;
						fprintf(csvFile, "%c", byteList[i]);
						break;
					}
					case NC_SHORT:
					{
						short *byteList = (short *)variableData->data;
						fprintf(csvFile, "%d", byteList[i]);
						break;
					}
					case NC_INT:
					{
						int *byteList = (int *)variableData->data;
						fprintf(csvFile, "%d", byteList[i]);
						break;
					}
					case NC_FLOAT:
					{
						float *byteList = (float *)variableData->data;
						fprintf(csvFile, "%f", byteList[i]);
						break;
					}
					case NC_DOUBLE:
					{
						double *byteList = (double *)variableData->data;
						fprintf(csvFile, "%f", byteList[i]);
						break;
					}
					default:
						break;
				}
				
				if (j != (numVars-1)) fprintf(csvFile, ", ");
			}
			fprintf(csvFile, "\r\n");
		}
		
		//free up heap memory
		for (i=0; i<numVars; i++)
		{
			free(standardNameList[i]);
		}
		free(standardNameList);
		for (i=0; i<numVars; i++)
		{
			free(longNameList[i]);
		}
		free(longNameList);
		for (i=0; i<numVars; i++)
		{
			free(unitStringList[i]);
		}
		free(unitStringList);
		for (i=0; i<numVars; i++)
		{
			free(variableDataList[i]->data);
		}
		free(variableDataList);
		free(csvFilename);
		
		//close the CSV file
		fclose(csvFile);
		
		//close the NetCDF file
		ncResult = nc_close(datasetID);
		if (ncResult != NC_NOERR) HandleNCError("nc_close", ncResult);
		
		printf("\r\n");
		
	}//end of input file for loop
	
	/*DIR *dp;
	struct dirent *ep;
	dp = opendir ("./");

	if (dp != NULL)
	{
		while (ep = readdir (dp))
			puts (ep->d_name);
		
		(void) closedir (dp);
	}
	else
	perror ("Couldn't open the directory");*/

	return 0;
}
