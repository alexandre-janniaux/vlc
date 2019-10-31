Vue.component('main-view', {
    template: '#main-view-template',
    computed: {
        ...Vuex.mapState({
            mainView: state => state.layout.mainView,
        })
    },
    methods: {
        setLibraryView() {
            this.$store.dispatch('layout/setLibraryView')
        }
    }
});
