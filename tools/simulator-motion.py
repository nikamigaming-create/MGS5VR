"""Continuous native OpenXR rehearsal motion, sampled against elapsed time."""
import bisect
import math


class MotionCurve:
    """C2 Hermite motion through timed poses, including zero-speed endpoints.

    Adjacent quaternion signs are joined before interpolating and normalizing.
    A delayed input submission advances along the curve instead of replaying
    obsolete samples and stretching every subsequent movement.
    """
    def __init__(self, points):
        if len(points) < 2:
            raise ValueError('A motion needs at least two timed poses')
        self.times = [float(p['time']) for p in points]
        if self.times[0] != 0 or any(b <= a for a, b in zip(self.times, self.times[1:])):
            raise ValueError('Motion times must increase from zero')
        self.values = []
        for point in points:
            q = list(point['orientation'])
            norm = math.sqrt(sum(x*x for x in q))
            if not .99 < norm < 1.01:
                raise ValueError('Motion orientations must be unit quaternions')
            q = [x/norm for x in q]
            if self.values and sum(a*b for a, b in zip(self.values[-1][3:7], q)) < 0:
                q = [-x for x in q]
            value = list(point['position']) + q + [point.get('squeeze', 0), point.get('trigger', 0)]
            if len(value) != 9 or not all(math.isfinite(x) for x in value):
                raise ValueError('Invalid motion pose')
            self.values.append(value)
        self.velocities = [[0.] * 9 for _ in points]
        self.accelerations = [[0.] * 9 for _ in points]
        for i in range(1, len(points)-1):
            before, after = self.times[i]-self.times[i-1], self.times[i+1]-self.times[i]
            for axis in range(9):
                incoming = (self.values[i][axis]-self.values[i-1][axis])/before
                outgoing = (self.values[i+1][axis]-self.values[i][axis])/after
                self.velocities[i][axis] = (incoming*after+outgoing*before)/(before+after)
                self.accelerations[i][axis] = 2*(outgoing-incoming)/(before+after)

    @property
    def duration(self):
        return self.times[-1]

    def sample(self, elapsed):
        elapsed = min(self.duration, max(0., elapsed))
        i = min(len(self.times)-2, bisect.bisect_right(self.times, elapsed)-1)
        h = self.times[i+1]-self.times[i]
        u = (elapsed-self.times[i])/h
        value = []
        for axis in range(9):
            c0 = self.values[i][axis]
            c1 = self.velocities[i][axis]*h
            c2 = self.accelerations[i][axis]*h*h*.5
            delta = self.values[i+1][axis]-c0-c1-c2
            velocity = self.velocities[i+1][axis]*h-c1-2*c2
            acceleration = self.accelerations[i+1][axis]*h*h-2*c2
            c3 = 10*delta-4*velocity+.5*acceleration
            c4 = -15*delta+7*velocity-acceleration
            c5 = 6*delta-3*velocity+.5*acceleration
            value.append(c0+u*(c1+u*(c2+u*(c3+u*(c4+u*c5)))))
        norm = math.sqrt(sum(x*x for x in value[3:7]))
        if norm < .1:
            raise ValueError('Motion rotation passes through a singularity')
        return dict(position=value[:3], orientation=[x/norm for x in value[3:7]],
                    squeeze=min(1., max(0., value[7])), trigger=min(1., max(0., value[8])))
