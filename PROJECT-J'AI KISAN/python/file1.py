import os
import sys
import serial  # Correct import for pyserial
import pymysql
from datetime import datetime
from tensorflow.keras.models import load_model
from tensorflow.keras.preprocessing import image
import numpy as np
import tensorflow as tf
from tensorflow.keras.preprocessing.image import ImageDataGenerator
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Conv2D, MaxPooling2D, Flatten, Dense

# Setting environment variable to manage TensorFlow operations
os.environ['TF_ENABLE_ONEDNN_OPTS'] = '0'
sys.stdout.reconfigure(encoding='utf-8')  # Ensure the default encoding is set to UTF-8

# Load and Preprocess the Data
base_dir = r'C:\Users\DR.RUDRA\Desktop\project-ieee\dataset'

datagen = ImageDataGenerator(
    rescale=1.0/255.0,
    validation_split=0.2  # Splitting the data into training and validation sets
)

train_generator = datagen.flow_from_directory(
    base_dir,
    target_size=(150, 150),
    batch_size=32,
    class_mode='binary',
    subset='training'  # Training data subset
)

validation_generator = datagen.flow_from_directory(
    base_dir,
    target_size=(150, 150),
    batch_size=32,
    class_mode='binary',
    subset='validation'  # Validation data subset
)

# Build the Convolutional Neural Network (CNN) Model
model = Sequential([
    Conv2D(32, (3, 3), activation='relu', input_shape=(150, 150, 3)),
    MaxPooling2D((2, 2)),
    Conv2D(64, (3, 3), activation='relu'),
    MaxPooling2D((2, 2)),
    Conv2D(128, (3, 3), activation='relu'),
    MaxPooling2D((2, 2)),
    Flatten(),
    Dense(512, activation='relu'),
    Dense(1, activation='sigmoid')  # Sigmoid activation for binary classification
])

# Compile and Train the Model
model.compile(optimizer='adam', loss='binary_crossentropy', metrics=['accuracy'])

# Train the model
history = model.fit(
    train_generator,
    steps_per_epoch=train_generator.samples // train_generator.batch_size,
    validation_data=validation_generator,
    validation_steps=validation_generator.samples // validation_generator.batch_size,
    epochs=25
)

# Evaluate the Model
loss, accuracy = model.evaluate(validation_generator)
print(f'Validation accuracy: {accuracy * 100:.2f}%')

# Save the Trained Model using the native Keras format
model.save('fish_health_classifier.keras')

# Database connection using pymysql (adjust credentials as needed)
connection = pymysql.connect(
    host='localhost',
    user='root',
    password='',  # Enter the password you set for MySQL in XAMPP
    database='aquaponics'
)

# Load the trained model
model = load_model('fish_health_classifier.keras')

# Function to predict health status
def predict_health(img_path):
    img = image.load_img(img_path, target_size=(150, 150))
    img_array = image.img_to_array(img) / 255.0
    img_array = np.expand_dims(img_array, axis=0)
    prediction = model.predict(img_array)
    print(f'Prediction: {prediction}')  # Debug print
    return 'Healthy' if prediction[0] >= 0.5 else 'Unhealthy'

# Function to save sensor data to database
def save_sensor_data(temp, ph, light, feed_time):
    cursor = connection.cursor()
    sql = "INSERT INTO sensor_data (temperature, ph, light_intensity, feed_time) VALUES (%s, %s, %s, %s)"
    cursor.execute(sql, (temp, ph, light, feed_time))
    connection.commit()

# Function to save health status to database
def save_health_status(img_path, health_status):
    cursor = connection.cursor()
    sql = "INSERT INTO fish_health (image_path, health_status) VALUES (%s, %s)"
    cursor.execute(sql, (img_path, health_status))
    connection.commit()

# Read data from Arduino
ser = serial.Serial('COM3', 115200, timeout=1)  # Adjust COM port as needed

while True:
    line = ser.readline().decode('utf-8').strip()
    if line:
        print(f"Received: {line}")
        parts = line.split(", ")
        if len(parts) == 3:
            temperature = float(parts[0].split(": ")[1])
            ph = int(parts[1].split(": ")[1])
            light_intensity = int(parts[2].split(": ")[1])
            feed_time = datetime.now()
            
            # Save sensor data to database
            save_sensor_data(temperature, ph, light_intensity, feed_time)
            print("Sensor data saved.")  # Debug print
            
            # Process and analyze images from the 'fish' folder
            fish_folder = r'C:\\Users\\DR.RUDRA\\Desktop\\project-ieee\\cap_image'
            for img_file in os.listdir(fish_folder):
                img_path = os.path.join(fish_folder, img_file)
                health_status = predict_health(img_path)
                save_health_status(img_path, health_status)
                print(f'The fish in {img_file} is {health_status}')  # Debug print

# Close the database connection
connection.close()
