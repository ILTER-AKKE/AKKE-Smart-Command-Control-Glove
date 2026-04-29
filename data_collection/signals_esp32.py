import numpy as np
from sklearn.preprocessing import scale
from scipy.interpolate import interp1d

class SampleESP32:
    def __init__(self, f1, f2, f3, f4, f5, pitch, roll, ax, ay, az, gx, gy, gz):
        self.f1 = f1
        self.f2 = f2
        self.f3 = f3
        self.f4 = f4
        self.f5 = f5
        self.pitch = pitch
        self.roll = roll
        self.ax = ax
        self.ay = ay
        self.az = az
        self.gx = gx
        self.gy = gy
        self.gz = gz

    def get_linearized(self, reshape=False):
        arr = np.concatenate((
            self.f1, self.f2, self.f3, self.f4, self.f5, 
            self.pitch, self.roll, 
            self.ax, self.ay, self.az, 
            self.gx, self.gy, self.gz
        ))
        if reshape:
            return arr.reshape(1, -1)
        else:
            return arr

    @staticmethod
    def load_from_csv(filename, size_fit=50):
        # We skip the first row (header) and ignore the timestamp column (index 0)
        data = np.genfromtxt(filename, delimiter=',', skip_header=1)
        
        if len(data) < 5:
            return None # Skip too short data
            
        # Data columns: timestamp, f1, f2, f3, f4, f5, pitch, roll, ax, ay, az, gx, gy, gz
        # We drop timestamp
        data = data[:, 1:]

        # Standardize the data by scaling it (exactly as the original author did)
        data_norm = scale(data)

        # Create a function for each axe that interpolates the samples
        x = np.linspace(0, data.shape[0], data.shape[0])
        
        f_f1 = interp1d(x, data_norm[:, 0], kind='linear')
        f_f2 = interp1d(x, data_norm[:, 1], kind='linear')
        f_f3 = interp1d(x, data_norm[:, 2], kind='linear')
        f_f4 = interp1d(x, data_norm[:, 3], kind='linear')
        f_f5 = interp1d(x, data_norm[:, 4], kind='linear')
        f_pitch = interp1d(x, data_norm[:, 5], kind='linear')
        f_roll = interp1d(x, data_norm[:, 6], kind='linear')
        f_ax = interp1d(x, data_norm[:, 7], kind='linear')
        f_ay = interp1d(x, data_norm[:, 8], kind='linear')
        f_az = interp1d(x, data_norm[:, 9], kind='linear')
        f_gx = interp1d(x, data_norm[:, 10], kind='linear')
        f_gy = interp1d(x, data_norm[:, 11], kind='linear')
        f_gz = interp1d(x, data_norm[:, 12], kind='linear')

        # Create a new sample set with the desired sample size by rescaling
        xnew = np.linspace(0, data.shape[0], size_fit)
        
        return SampleESP32(
            f_f1(xnew), f_f2(xnew), f_f3(xnew), f_f4(xnew), f_f5(xnew),
            f_pitch(xnew), f_roll(xnew),
            f_ax(xnew), f_ay(xnew), f_az(xnew),
            f_gx(xnew), f_gy(xnew), f_gz(xnew)
        )
