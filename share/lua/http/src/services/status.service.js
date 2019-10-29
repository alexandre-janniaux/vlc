import langUtils from '../utils/lang/index.js';

export default {
    sendGetStatus(params) {
        return $.ajax({
            url: 'requests/status.json',
            data: params
        })
        .then((data) => {
            return JSON.parse(data);
        });
    },
    fetchStatus() {
        return this.sendGetStatus();
    },
    toggleRandom() {
        return this.sendGetStatus('command=pl_random');
    },
    toggleRepeat() {
        return this.sendGetStatus('command=pl_repeat');
    },
    play(id) {
        const idParam = `&id=${id}`;
        return this.sendGetStatus(`command=pl_play${langUtils.isPresent(id) && id !== -1 ? idParam : ''}`);
    },
    pause() {
        return this.sendGetStatus('command=pl_pause');
    },
    updateVolume(volume) {
        const cmd = `command=volume&val=${volume}`;
        return this.sendGetStatus(cmd);
    }
};
