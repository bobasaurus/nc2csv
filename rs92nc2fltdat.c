//rs92nc2fltdat.c: Convert a GRUAN RS-92 NetCDF file into a balloon.pro-compatible flt.dat file
//Copyright 2012, Allen Jordan, allen.jordan@gmail.com

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <netcdf.h>

#define VERSION		1.001

//a macro for copying a substring
//#define substr(dest, src, start, length) (strlcpy(dest, src+start, length+1))
#define substr(dest, src, start, length) (snprintf(dest, length, "%s", src))

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
		
		//allocate space for the flt.dat filename, plus some room for the longer extension, etc
		char *fltDatFilename = malloc((filenameLength + 7)*sizeof(char));
		strcpy(fltDatFilename, filename);
		char *periodLocation = strrchr(fltDatFilename, '.');
		*periodLocation = '\0';
		strcat(fltDatFilename, "flt.dat");
		
		//open the NetCDF file/dataset
		int datasetID;
		int ncResult;
		ncResult = nc_open(filename, NC_NOWRITE, &datasetID);
		if (ncResult != NC_NOERR) HandleNCError("nc_open", ncResult);
		
		printf("opened NetCDF file: %s", filename);
		printf("output flt.dat filename: %s\n", fltDatFilename);
		
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
		
		//open/create the flt.dat file for outputting data
		//todo: better file name
		FILE *fltFile = fopen(fltDatFilename, "w");
		
		//get the current GMT date/time
		time_t currentTime;
		time(&currentTime);
		struct tm *currentTimeStruct = gmtime(&currentTime);
		
		printf("current gmt time: %d/%d/%d %d:%d:%d\n", currentTimeStruct->tm_year+1900, currentTimeStruct->tm_mon+1, currentTimeStruct->tm_mday, 
			currentTimeStruct->tm_hour, currentTimeStruct->tm_min, currentTimeStruct->tm_sec);
		
		//get the launch date/time attribute
		char launchTimeStr[100];
		ncResult = nc_get_att_text(datasetID, NC_GLOBAL, "g.Ascent.StartTime", launchTimeStr);
		if (ncResult != NC_NOERR) HandleNCError("nc_get_att_text", ncResult);
		//split the launch date/time string into token strings
		char yearStr[10];
		substr(yearStr, launchTimeStr, 0, 4);
		char monStr[10];
		substr(monStr, launchTimeStr, 5, 2);
		char dayStr[10];
		substr(dayStr, launchTimeStr, 8, 2);
		char hourStr[10];
		substr(hourStr, launchTimeStr, 11, 2);
		char minStr[10];
		substr(minStr, launchTimeStr, 14, 2);
		char secStr[10];
		substr(secStr, launchTimeStr, 17, 2);
		//convert the launch date/time into a tm structure
		struct tm launchTimeStruct;
		int launchYear = atoi(yearStr);
		int launchMonth = atoi(monStr);
		int launchDay = atoi(dayStr);
		int launchHour = atoi(hourStr);
		int launchMinute = atoi(minStr);
		int launchSecond = atoi(secStr);
		
		printf("launch gmt time: %d/%d/%d %d:%d:%d\n", launchYear, launchMonth, launchDay, launchHour, launchMinute, launchSecond);
		
		//output the flt.dat header
		fprintf(fltFile, "Extended NOAA/GMD preliminary data %02d-%02d-%04d %02d:%02d:%02d [GMT], nc2fltdat version %.3f\r\n", 
			currentTimeStruct->tm_mday, currentTimeStruct->tm_mon+1, currentTimeStruct->tm_year+1900, currentTimeStruct->tm_hour, currentTimeStruct->tm_min, currentTimeStruct->tm_sec, VERSION);
		
		fprintf(fltFile, "Software written by Allen Jordan, NOAA\r\n");
		fprintf(fltFile, "               Header lines = 16\r\n");
		fprintf(fltFile, "               Data columns = 12\r\n");
		fprintf(fltFile, "                 Date [GMT] = %02d-%02d-%04d\r\n", launchDay, launchMonth, launchYear);
		fprintf(fltFile, "                 Time [GMT] = %02d:%02d:%02d\r\n", launchHour, launchMinute, launchSecond);
		fprintf(fltFile, "            Instrument type = Vaisala RS92\r\n");
		fprintf(fltFile, "\r\n\r\n");
		fprintf(fltFile, "    THE DATA CONTAINED IN THIS FILE ARE PRELIMINARY\r\n");
		fprintf(fltFile, "     AND SUBJECT TO REPROCESSING AND VERIFICATION\r\n");
		fprintf(fltFile, "\r\n\r\n\r\n");
		fprintf(fltFile, "      Time,     Press,       Alt,      Temp,        RH,     TFp V,   GPS lat,   GPS lon,   GPS alt,      Wind,  Wind Dir,        Fl\r\n");
		fprintf(fltFile, "     [min],     [hpa],      [km],   [deg C],       [%%],   [deg C],     [deg],     [deg],      [km],     [m/s],     [deg],        []\r\n");
		
		//storage for all variables in the NetCDF file
		//todo: watch out for segfaults, maybe use nc_get_vara_ to get pieces instead of whole variables
		VariableData **variableDataList = (VariableData **)malloc(numVars * sizeof(VariableData*));
		char **standardNameList = (char **)malloc(numVars * sizeof(char*));
		//char **longNameList = (char **)malloc(numVars * sizeof(char*));
		//char **unitStringList = (char **)malloc(numVars * sizeof(char*));
		
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
			
			//output variable name to the flt.dat file
			//fprintf(fltFile, "%s", varName);
			//if (varID != (numVars-1)) fprintf(fltFile, ", ");
			
			/*//see if there is an attribute for the variable standard name, and store it if there is
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
			}*/
			
			
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
		
		//output a newline after the variable names flt.dat header line
		//fprintf(fltFile, "\r\n");
		
		/*//output the variable standard names
		for (i=0; i<numVars; i++)
		{
			fprintf(fltFile, "%s", standardNameList[i]);
			if (i != (numVars-1)) fprintf(fltFile, ", ");
		}
		fprintf(fltFile, "\r\n");
		
		//output the variable long names
		for (i=0; i<numVars; i++)
		{
			fprintf(fltFile, "%s", longNameList[i]);
			if (i != (numVars-1)) fprintf(fltFile, ", ");
		}
		fprintf(fltFile, "\r\n");
		
		//output the variable units
		for (i=0; i<numVars; i++)
		{
			char *unitsName = unitStringList[i];
			if (unitsName[0] != '[')
				fprintf(fltFile, "[");
			
			fprintf(fltFile, "%s", unitsName);
			
			if (unitsName[strlen(unitsName)-1] != ']')
				fprintf(fltFile, "]");
			
			if (i != (numVars-1)) fprintf(fltFile, ", ");
		}
		fprintf(fltFile, "\r\n");*/
		
		
		
		
		
		
		
		int timeIndex;
		ncResult = nc_inq_varid(datasetID, "time", &timeIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (time)", ncResult);
		int pressureIndex;
		ncResult = nc_inq_varid(datasetID, "press", &pressureIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (press)", ncResult);
		int temperatureIndex;
		ncResult = nc_inq_varid(datasetID, "temp", &temperatureIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (temp)", ncResult);
		int vaisRHIndex;
		ncResult = nc_inq_varid(datasetID, "rh", &vaisRHIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (rh)", ncResult);
		int windDirIndex;
		ncResult = nc_inq_varid(datasetID, "wdir", &windDirIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (wdir)", ncResult);
		int windSpeedIndex;
		ncResult = nc_inq_varid(datasetID, "wspeed", &windSpeedIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (wspeed)", ncResult);
		int geopotAltIndex;
		ncResult = nc_inq_varid(datasetID, "geopot", &geopotAltIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (geopot)", ncResult);
		int lonIndex;
		ncResult = nc_inq_varid(datasetID, "lon", &lonIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (lon)", ncResult);
		int latIndex;
		ncResult = nc_inq_varid(datasetID, "lat", &latIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (lat)", ncResult);
		int gpsAltIndex;
		ncResult = nc_inq_varid(datasetID, "alt", &gpsAltIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (alt)", ncResult);
		int vaisFPIndex;
		ncResult = nc_inq_varid(datasetID, "FP", &vaisFPIndex);
		if (ncResult != NC_NOERR) HandleNCError("nc_inq_varid (FP)", ncResult);
		
		for (i = 0; i < dimLength; i++)
		{
			if (variableDataList[timeIndex]->type != NC_FLOAT) 
			{
				printf("Invalid NetCDF type for outputting to flt.dat: %d\r\n", variableDataList[timeIndex]->type);
			}
			fprintf(fltFile, "%10.5f,%10.2f,%10.4f,%10.2f,%10.2f,%10.2f,%10.5f,%10.5f,%10.4f,%10.2f,%10.2f,%10d\r\n", 
				((float*)variableDataList[timeIndex]->data)[i] / 60.0, ((float*)variableDataList[pressureIndex]->data)[i], ((float*)variableDataList[geopotAltIndex]->data)[i] / 1000,
				((float*)variableDataList[temperatureIndex]->data)[i] - 273.15, ((float*)variableDataList[vaisRHIndex]->data)[i]*100, ((float*)variableDataList[vaisFPIndex]->data)[i] - 273.15,
				((float*)variableDataList[latIndex]->data)[i], ((float*)variableDataList[lonIndex]->data)[i], ((float*)variableDataList[gpsAltIndex]->data)[i]/1000, ((float*)variableDataList[windSpeedIndex]->data)[i], ((float*)variableDataList[windDirIndex]->data)[i], 1);
		}
		
		/*
		//output variable data to the flt.dat file
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
						fprintf(fltFile, "%u", byteList[i]);
						break;
					}
					case NC_CHAR:
					{
						char *byteList = (char *)variableData->data;
						fprintf(fltFile, "%c", byteList[i]);
						break;
					}
					case NC_SHORT:
					{
						short *byteList = (short *)variableData->data;
						fprintf(fltFile, "%d", byteList[i]);
						break;
					}
					case NC_INT:
					{
						int *byteList = (int *)variableData->data;
						fprintf(fltFile, "%d", byteList[i]);
						break;
					}
					case NC_FLOAT:
					{
						float *byteList = (float *)variableData->data;
						fprintf(fltFile, "%f", byteList[i]);
						break;
					}
					case NC_DOUBLE:
					{
						double *byteList = (double *)variableData->data;
						fprintf(fltFile, "%f", byteList[i]);
						break;
					}
					default:
						break;
				}
				
				if (j != (numVars-1)) fprintf(fltFile, ", ");
			}
			fprintf(fltFile, "\r\n");
		}*/
		
		//free up heap memory
		/*for (i=0; i<numVars; i++)
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
		free(unitStringList);*/
		for (i=0; i<numVars; i++)
		{
			free(variableDataList[i]->data);
		}
		free(variableDataList);
		free(fltDatFilename);
		
		//close the flt.dat file
		fclose(fltFile);
		
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
