from  tkinter import *
from  tkinter import  ttk
from tkinter import messagebox
import os
import glob
import time
import cv2
import face_recognition
import serial
import serial.tools.list_ports

def open_test():
            known_faces = []
            known_names = []
            known_faces_paths = []
            registered_faces_path = 'dataset/'
            for name in os.listdir(registered_faces_path):
                images_mask = '%s%s/*.jpg' % (registered_faces_path, name)
                images_paths = glob.glob(images_mask)
                known_faces_paths += images_paths
                known_names += [name for x in images_paths]

            def get_encodings(img_path):
                image = face_recognition.load_image_file(img_path)
                encoding = face_recognition.face_encodings(image)
                return encoding[0]

            known_faces = [get_encodings(img_path) for img_path in known_faces_paths]
            # Camera selection ________________
            vc = cv2.VideoCapture(0)
            prev_frame_time = 0
            new_frame_time = 0
            ports = serial.tools.list_ports.comports()
            varser = len(ports), 'ports found'
            if len(ports)==0:
                messagebox.showinfo(title='info', message=varser)
            else:
                messagebox.showinfo(title='info', message=varser)
                for port in ports:
                    messagebox.showinfo(title='info', message=port.device)
                    ser = serial.Serial(port.device, 9600)
                    messagebox.showinfo(title='info', message="START")
                    while True:
                        ret, frame = vc.read()
                        if not ret:
                            break
                        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                        faces = face_recognition.face_locations(frame_rgb)
                        font = cv2.FONT_HERSHEY_SIMPLEX
                        if len(faces) == 1:
                            for face in faces:
                                top, right, bottom, left = face
                                cv2.rectangle(frame, (left, top), (right, bottom), (0, 0, 255), 2)
                                face_code = face_recognition.face_encodings(frame_rgb, [face])[0]
                                results = face_recognition.compare_faces(known_faces, face_code, tolerance=0.6)
                                if any(results):
                                    datasend = "ON"
                                    datasend = datasend + "\r"
                                    ser.write(datasend.encode())

                                    name = known_names[results.index(True)]
                                    cv2.putText(frame, name, (left, bottom + 20), cv2.FONT_HERSHEY_PLAIN, 2,
                                                (0, 255, 0), 2)
                                else:
                                    datasend = "OFF"
                                    datasend = datasend + "\r"
                                    ser.write(datasend.encode())

                                    name = 'UNKNOW '
                                    cv2.putText(frame, name, (left, bottom + 20), cv2.FONT_HERSHEY_PLAIN, 2,
                                                (0, 0, 255), 2)
                        elif len(faces) > 0:

                            datasend = "OFF"
                            datasend = datasend + "\r"
                            ser.write(datasend.encode())

                            cv2.rectangle(img=frame, pt1=(15, 70), color=(10, 200, 0), pt2=(543, 120), thickness=5)
                            cv2.putText(frame, 'Please one person', (20, 110), cv2.FONT_HERSHEY_PLAIN, 3, (0, 0, 255),
                                        5)
                        else:

                            datasend = "OFF"
                            datasend = datasend + "\r"
                            ser.write(datasend.encode())

                            cv2.rectangle(img=frame, pt1=(85, 175), color=(10, 200, 0), pt2=(550, 300), thickness=3)
                            cv2.putText(frame, 'there is no one', (124, 245), cv2.FONT_HERSHEY_PLAIN, 3, (0, 0, 255), 2)
                        cv2.imshow('face recognition', frame)
                        k = cv2.waitKey(1)
                        # The program closes if you press the letter q on the keyboard
                        if ord('q') == k:
                            break
                    cv2.destroyAllWindows()
                    vc.release()



def win2():
    def dataknow():
        data_image_get = data_image.get()
        if data_image_get == 'images':
            messagebox.showinfo(title='info', message=f'\n  | select number of images | ')
        else:
            messagebox.showinfo(title='info', message=f'\n  | selected successfully | ')
            cam = cv2.VideoCapture(0)
            detector = cv2.CascadeClassifier('haarcascade_frontalface_default.xml')
            sampleNum = 0
            while (True):
                ret, img = cam.read()
                faces = detector.detectMultiScale(img, 1.3, 5)
                for (x, y, w, h) in faces:

                    sampleNum = sampleNum + 1
                    cv2.imwrite("dataset/know/ " + str(sampleNum) + ".jpg",
                                img[y:y + h, x:x + w])
                    cv2.imshow('Frame', img)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
                elif sampleNum > int(data_image_get):
                    break
            cam.release()
            cv2.destroyAllWindows()
            label = Label(text=' Images Saved ', bg="#007660", font=('times', 30, 'bold')).place(x=390, y=480)
    app_width = 988
    app_height = 586
    window2 = Tk()
    width = window2.winfo_screenwidth()
    height = window2.winfo_screenheight()
    x = (width / 2) - (app_width / 2)
    y = (height / 2) - (app_height / 2)
    window2.geometry(f"{app_width}x{app_height}+{int(x)}+{int(y)}")
    window2.title("SALIM DEV")
    window2.resizable(False, False)
    window2.iconbitmap('app/icon.ico')
    filename = PhotoImage(file="app/2.png")
    background_label = Label(window2, image=filename)
    dataknow_image = PhotoImage(file="app/dataknow1.png")
    back_image = PhotoImage(file="app/back.png")
    image = ["images",
             "100",
             "200",
             "300",
             "400",
             "500",
             "1000",
             "2000",
             "3000",
             "5000",
             "7000",
             "10000", ]
    data_image = ttk.Combobox(values=image, height=50, state="readonly", width=10, font=('Consolas', 12),background="#007660",foreground="#007660",)
    data_image.set(image[0])
    data_image.place(x=460, y=400)
    dataknowmodel = Button(image=dataknow_image,border=0,borderwidth=0,background="#007660", command=dataknow).place(x=390, y=350)
    back_button = Button(image=back_image, border=0, borderwidth=0, background="#007660", command=lambda: [window2.destroy(), win1()]).place(x=880, y=500)
    background_label.pack()
    window2.mainloop()
def win1():
    app_width = 990
    app_height = 587
    win1 = Tk()
    width = win1.winfo_screenwidth()
    height = win1.winfo_screenheight()
    x = (width / 2) - (app_width / 2)
    y = (height / 2) - (app_height / 2)
    win1.geometry(f"{app_width}x{app_height}+{int(x)}+{int(y)}")
    win1.title("SALIM DEV")
    win1.resizable(False, False)
    win1.iconbitmap('app/icon.ico')
    filename = PhotoImage(file="app/1.png")
    background_label = Label(win1, image=filename)
    train_model_image = PhotoImage(file="app/train_model1.png")
    test_model_image = PhotoImage(file="app/test_model1.png")
    button_train_model = Button(image=train_model_image,border=0,borderwidth=0,background="#007660", command=lambda: [win1.destroy(), win2()]).place(x=664, y=280)
    button_test_model = Button(image=test_model_image,border=0,borderwidth=0,background="#007660", command=open_test).place(x=664, y=395)
    background_label.pack()
    win1.mainloop()
win1()
