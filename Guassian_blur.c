#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "lodepng.h"
#include "lodepng.c"

#define NUM_THREADS 4

// Thread arguments structure
typedef struct
{
    unsigned char *inputImage;
    unsigned char *outputImage;
    int width;
    int height;
    int startY;
    int endY;
} ThreadArgs;

// Function to filter a portion of the input image in parallel
void *filterImageThread(void *args);

// Function to filter the input image
void filterImage(unsigned char *inputImage, unsigned char *outputImage, int width, int height);

int main()
{
    unsigned error;
    unsigned char *image;
    unsigned int width, height;

    // Decode the input image
    error = lodepng_decode32_file(&image, &width, &height, "Input.png");
    if (error)
    {
        printf("Error decoding image %u: %s", error, lodepng_error_text(error));
        return 1;
    }

    // Allocate memory for the output image
    unsigned char *outputImage = (unsigned char *)malloc(4 * width * height * sizeof(unsigned char));

    // Apply the filter to the image using multithreading
    filterImage(image, outputImage, width, height);

    // Encode and save the filtered image
    error = lodepng_encode32_file("Output.png", outputImage, width, height);
    if (error)
    {
        printf("Error while encoding image %u: %s", error, lodepng_error_text(error));
    }
    else
    {
        printf("Image saved.");
    }

    // Free allocated memory
    free(image);
    free(outputImage);

    return 0;
}

// Function to filter the input image using multithreading
void filterImage(unsigned char *inputImage, unsigned char *outputImage, int width, int height)
{
    // Initialize thread-related variables
    pthread_t threads[NUM_THREADS];
    ThreadArgs threadArgs[NUM_THREADS];

    // Calculate the number of rows each thread will process
    int rowsPerThread = height / NUM_THREADS;
    int remainingRows = height % NUM_THREADS;

    // Start threads to process different portions of the image
    for (int i = 0; i < NUM_THREADS; i++)
    {
        threadArgs[i].inputImage = inputImage;
        threadArgs[i].outputImage = outputImage;
        threadArgs[i].width = width;
        threadArgs[i].height = height;

        // Calculate startY and endY for each thread
        threadArgs[i].startY = i * rowsPerThread;
        threadArgs[i].endY = (i + 1) * rowsPerThread;

        // Include remaining rows in the last thread
        if (i == NUM_THREADS - 1)
        {
            threadArgs[i].endY += remainingRows;
        }

        // Create threads
        pthread_create(&threads[i], NULL, filterImageThread, &threadArgs[i]);
    }

    // Join threads to wait for their completion
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

// Function to filter a portion of the input image in parallel
void *filterImageThread(void *args)
{
    // Extract thread arguments
    ThreadArgs *threadArgs = (ThreadArgs *)args;

    // Unpack arguments
    unsigned char *inputImage = threadArgs->inputImage;
    unsigned char *outputImage = threadArgs->outputImage;
    int width = threadArgs->width;
    int height = threadArgs->height;
    int startY = threadArgs->startY;
    int endY = threadArgs->endY;

    // Loop through each pixel in the specified range
    for (int tidY = startY; tidY < endY; tidY++)
    {
        for (int tidX = 0; tidX < width; tidX++)
        {
            int count = 0;
            int redSum = 0, greenSum = 0, blueSum = 0;

            // Iterate through the 3x3 neighborhood of the current pixel
            for (int i = tidY - 1; i <= tidY + 1; i++)
            {
                for (int j = tidX - 1; j <= tidX + 1; j++)
                {
                    // Check if the indices are within the image bounds
                    if (i >= 0 && i < height && j >= 0 && j < width)
                    {
                        // Accumulate color values and count
                        redSum += inputImage[4 * (i * width + j) + 0];
                        greenSum += inputImage[4 * (i * width + j) + 1];
                        blueSum += inputImage[4 * (i * width + j) + 2];
                        count++;
                    }
                }
            }

            // Apply different weightings based on the pixel position

            // Corner pixels
            if ((tidY == 0 && tidX == 0) || (tidY == 0 && tidX == width - 1) ||
                (tidY == height - 1 && tidX == 0) || (tidY == height - 1 && tidX == width - 1))
            {
                outputImage[4 * (tidY * width + tidX) + 0] = (redSum / 1.5) / 4;
                outputImage[4 * (tidY * width + tidX) + 1] = (greenSum / 1.5) / 4;
                outputImage[4 * (tidY * width + tidX) + 2] = (blueSum / 1.5) / 4;
                outputImage[4 * (tidY * width + tidX) + 3] = inputImage[4 * (tidY * width + tidX) + 3];
            }
            // Edge pixels
            else if ((tidY == 0 && tidX < width - 1) || (tidY < height - 1 && tidX == width - 1) ||
                     (tidY == height - 1 && tidX < width - 1) || (tidY < height - 1 && tidX == 0))
            {
                outputImage[4 * (tidY * width + tidX) + 0] = (redSum / 1.5) / 6;
                outputImage[4 * (tidY * width + tidX) + 1] = (greenSum / 1.5) / 6;
                outputImage[4 * (tidY * width + tidX) + 2] = (blueSum / 1.5) / 6;
                outputImage[4 * (tidY * width + tidX) + 3] = inputImage[4 * (tidY * width + tidX) + 3];
            }
            // Interior pixels
            else
            {
                outputImage[4 * (tidY * width + tidX) + 0] = (redSum / 1.5) / 9;
                outputImage[4 * (tidY * width + tidX) + 1] = (greenSum / 1.5) / 9;
                outputImage[4 * (tidY * width + tidX) + 2] = (blueSum / 1.5) / 9;
                outputImage[4 * (tidY * width + tidX) + 3] = inputImage[4 * (tidY * width + tidX) + 3];
            }
        }
    }

    // Exit the thread
    pthread_exit(NULL);
}
