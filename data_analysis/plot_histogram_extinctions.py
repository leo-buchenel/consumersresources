import numpy as np
import matplotlib.pyplot as plt

nest = 0.1
conn = 0.0832

filename = 'histogram_extinctions_nest' + str(nest) + '_conn' + str(conn)
data = np.loadtxt('data_output/' + filename + ".out")
extinct = data[:, 0]
time_eq = data[:, 1]

title_string = 'Nestedness ' + str(nest) + ' Connectance ' + str(conn)
nbins = np.linspace(min(extinct) - 0.5, max(extinct) +
                    0.5, max(extinct) - min(extinct) + 2)
fig1 = plt.figure('Histogram extinctions')
ax1 = fig1.add_subplot(111)
ax1.hist(extinct, nbins, align='mid', density=1)
ax1.set_title(title_string)
ax1.set_xlabel(r'Number of extinctions at $\Delta = \Delta^*$')
ax1.set_ylabel(r'Probability')
fig1.savefig('plots/' + filename + '.pdf')


fig2 = plt.figure('Histogram time to reach equilibrium')
ax2 = fig1.add_subplot(111)
ax2.hist(teq, 100, align='mid', density=1)
ax2.set_title(title_string)
ax2.set_xlabel(
    r'Time to reach equilibrium after perturbation at $\Delta = \Delta^*$')
ax2.set_ylabel(r'Probability')
fig2.savefig('plots/' + 'histogram_teq_nest' +
             str(nest) + '_conn' + str(conn) + '.pdf')

plt.clf()
