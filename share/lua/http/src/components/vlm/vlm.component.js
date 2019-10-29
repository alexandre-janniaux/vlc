Vue.component('vlm-modal', {
    template: '#vlm-modal-template',
    methods: {
        executeVLM() {
            const cmd = $('#vlmCommand').val();
            this.$store.dispatch('vlmCmd/postCmd', cmd);
        }
    }
});
