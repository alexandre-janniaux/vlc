export default {
    isBlank(obj) {
        return obj === undefined || obj === null;
    },
    isPresent(obj) {
        return !this.isBlank(obj);
    }
};
