import timeUtils from '../time/index.js';

Vue.filter('formatDuration', (value) => {
    if (!value) {
        return '';
    }
    return timeUtils.formatTime(value);
});
